/* SPDX-License-Identifier: GPL-3.0-only
 * Exercise sound-design ranges through the public patch APIs. The numerical
 * assertions express broad acoustic differences, not a particular PCM hash.
 * Run with address/undefined/float-cast-overflow sanitizers to check DSP paths.
 */
#include "delxr_lxr_drum.h"
#include "delxr_lxr_snare.h"
#include "delxr_lxr_metal.h"
#include "delxr_lxr_patch.h"
#include "Oscillator.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { N = 16384, FFT_N = 4096, RATE = 44100 };
typedef union { DelxrLxrDrum drum; DelxrLxrSnare snare; DelxrLxrMetal metal; } Voice;
typedef struct { double rms, centroid, roughness, periodicity; } Metrics;
static const char* familyNames[] = {"drum", "snare", "cymbal", "closed hat", "open hat"};
static unsigned checks;
static int family;
static const char* stage;
static int16_t a[N], b[N], waves[6][N];
static float knobs[4] = {.7f, .5f, .7f, 0.f};
static float pitchRatio = 1.f;

#define CHECK(x, why) do { ++checks; if (!(x)) { \
    fprintf(stderr, "FAIL %s / %s: %s (line %d)\n", familyNames[family], stage, why, __LINE__); exit(1); \
} } while (0)

static void start(Voice* voice, const DelxrLxrPatch* patch) {
    if (family == DELXR_LXR_FAMILY_DRUM) {
        const DelxrLxrDrumControls c = {knobs[0],knobs[1],knobs[2],knobs[3]};
        delxr_lxr_drum_trigger_patch(&voice->drum, &c, patch);
    } else if (family == DELXR_LXR_FAMILY_SNARE) {
        const DelxrLxrSnareControls c = {knobs[0],knobs[1],knobs[2],knobs[3]};
        delxr_lxr_snare_trigger_patch(&voice->snare, &c, patch);
    } else {
        const DelxrLxrMetalControls c = {knobs[0],knobs[1],knobs[2],knobs[3]};
        delxr_lxr_metal_trigger_patch(&voice->metal, family - DELXR_LXR_FAMILY_CYMBAL, &c, patch);
    }
}

static int render(Voice* voice, const DelxrLxrPatch* patch, int16_t* out, int count) {
    if (family == DELXR_LXR_FAMILY_DRUM) {
        const DelxrLxrDrumControls c = {knobs[0],knobs[1],knobs[2],knobs[3]};
        return delxr_lxr_drum_render_patch(&voice->drum, &c, pitchRatio, out, count, patch);
    } else if (family == DELXR_LXR_FAMILY_SNARE) {
        const DelxrLxrSnareControls c = {knobs[0],knobs[1],knobs[2],knobs[3]};
        return delxr_lxr_snare_render_patch(&voice->snare, &c, pitchRatio, out, count, patch);
    } else {
        const DelxrLxrMetalControls c = {knobs[0],knobs[1],knobs[2],knobs[3]};
        return delxr_lxr_metal_render_patch(&voice->metal, &c, pitchRatio, out, count, patch);
    }
}

static void sound(const DelxrLxrPatch* patch, int16_t* out) {
    Voice voice;
    memset(&voice, 0xa5, sizeof(voice));
    start(&voice, patch);
    render(&voice, patch, out, N);
}

static void defaults(DelxrLxrPatch* patch) {
    delxr_lxr_patch_defaults(patch, family);
    knobs[0] = .7f; knobs[1] = .5f; knobs[2] = .7f; knobs[3] = 0.f;
    pitchRatio = 1.f;
}

static double energy(const int16_t* pcm, int count) {
    double total = 0.;
    for (int i = 0; i < count; ++i) total += (double)pcm[i] * pcm[i];
    return total;
}

static double difference(const int16_t* x, const int16_t* y, int count) {
    double delta = 0.;
    for (int i = 0; i < count; ++i) { double d = x[i] - y[i]; delta += d*d; }
    return delta / (energy(x, count) + energy(y, count) + 1.);
}

/* Windowed power-spectrum centroid, RMS, first-difference energy and normalized
 * autocorrelation. Radix-2 FFT keeps the suite independent of DSP libraries. */
static Metrics measure(const int16_t* pcm) {
    double real[FFT_N], imag[FFT_N] = {0}, sum = 0., derivative = 0.;
    for (int i = 0; i < FFT_N; ++i) {
        real[i] = pcm[i] * (.5 - .5*cos(2*M_PI*i/(FFT_N-1)));
        sum += (double)pcm[i]*pcm[i];
        if (i) { double d = pcm[i]-pcm[i-1]; derivative += d*d; }
    }
    for (unsigned i = 1, j = 0; i < FFT_N; ++i) {
        unsigned bit = FFT_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = real[i]; real[i] = real[j]; real[j] = t; }
    }
    for (int len = 2; len <= FFT_N; len *= 2) {
        const double wr = cos(-2*M_PI/len), wi = sin(-2*M_PI/len);
        for (int begin = 0; begin < FFT_N; begin += len) {
            double xr = 1., xi = 0.;
            for (int j = 0; j < len/2; ++j) {
                int p = begin+j, q = p+len/2;
                double vr = real[q]*xr-imag[q]*xi, vi = real[q]*xi+imag[q]*xr;
                real[q] = real[p]-vr; imag[q] = imag[p]-vi;
                real[p] += vr; imag[p] += vi;
                double nr = xr*wr-xi*wi; xi = xr*wi+xi*wr; xr = nr;
            }
        }
    }
    double weighted = 0., spectralEnergy = 0.;
    for (int i = 1; i < FFT_N/2; ++i) {
        double power = real[i]*real[i]+imag[i]*imag[i];
        weighted += power*i*RATE/FFT_N; spectralEnergy += power;
    }
    double periodicity = 0.;
    /* 440-Hz test tone has a ~100-sample period. Noise at the same sample-and-hold
     * clock must not become a repeat of that fundamental. */
    for (int lag = 95; lag <= 105; ++lag) {
        double cross = 0., left = 0., right = 0.;
        for (int i = lag; i < FFT_N; ++i) {
            cross += (double)pcm[i]*pcm[i-lag];
            left += (double)pcm[i]*pcm[i]; right += (double)pcm[i-lag]*pcm[i-lag];
        }
        double r = cross / (sqrt(left*right)+1.);
        if (r > periodicity) periodicity = r;
    }
    return (Metrics){sqrt(sum/FFT_N), weighted/(spectralEnergy+1.), derivative/(sum+1.), periodicity};
}

static void isolated_tone(DelxrLxrPatch* p) {
    defaults(p);
    p->value[DELXR_LXR_TRANSIENT_LEVEL] = 0;
    p->value[DELXR_LXR_MOD1_GAIN] = 0;
    p->value[DELXR_LXR_MOD2_GAIN] = 0;
    p->value[DELXR_LXR_CARRIER_WAVE] = 0;
    p->value[DELXR_LXR_CARRIER_TUNE] = 69;
    p->value[DELXR_LXR_FILTER_TYPE] = 7;
    p->value[DELXR_LXR_DISTORTION] = 0;
    p->value[DELXR_LXR_PITCH_AMOUNT] = 0;
    knobs[1] = 0.f;
    if (family == DELXR_LXR_FAMILY_SNARE) knobs[2] = 0.f;
}

static void test_waveforms(void) {
    stage = "waveform acoustic range";
    DelxrLxrPatch p;
    isolated_tone(&p);
    Metrics m[6];
    for (int wave = 0; wave < 6; ++wave) {
        p.value[DELXR_LXR_CARRIER_WAVE] = wave;
        sound(&p, waves[wave]);
        CHECK(energy(waves[wave], N) > 100000., "every carrier waveform must produce audio");
        m[wave] = measure(waves[wave] + 4096);
        for (int previous = 0; previous < wave; ++previous)
            CHECK(difference(waves[previous], waves[wave], N) > .0001, "carrier waveforms must not collapse together");
    }
    CHECK(m[0].periodicity > .9, "isolated sine must remain strongly periodic");
    CHECK(m[4].periodicity < .8, "pitched noise must have distinct non-periodic texture");
    CHECK(m[3].roughness > 2*m[0].roughness, "square must have stronger edges than sine");
    printf("  %-10s sine/noise periodicity %.3f/%.3f, sine/square edge energy %.5f/%.5f\n",
           familyNames[family], m[0].periodicity, m[4].periodicity, m[0].roughness, m[3].roughness);
    p.value[DELXR_LXR_CARRIER_WAVE] = 5;
    p.value[DELXR_LXR_PULSE_WIDTH] = 16; sound(&p, a);
    p.value[DELXR_LXR_PULSE_WIDTH] = 112; sound(&p, b);
    CHECK(difference(a,b,N) > .01, "PWM must offer a substantial duty-cycle range");
}

static void test_patch_isolation(void) {
    stage = "patch lifecycle and live editing";
    DelxrLxrPatch p;
    defaults(&p);
    p.value[DELXR_LXR_CARRIER_WAVE] = 5;
    p.value[DELXR_LXR_TRANSIENT_WAVE] = 11;
    p.value[DELXR_LXR_TRANSIENT_LEVEL] = 80;
    sound(&p, a);
    Voice v, distraction;
    start(&v, &p);
    start(&distraction, &p);
    int16_t unused[193];
    for (int offset = 0; offset < N;) {
        int count = 1+(offset*29)%257;
        if (count > N-offset) count = N-offset;
        render(&v,&p,b+offset,count);
        render(&distraction,&p,unused,193);
        offset += count;
    }
    CHECK(!memcmp(a,b,sizeof(a)), "patch voices must stay independent across arbitrary chunks");
    start(&v,&p); render(&v,&p,b,N);
    CHECK(!memcmp(a,b,sizeof(a)), "patch retrigger must reset oscillator/filter/transient state");

    isolated_tone(&p);
    p.value[DELXR_LXR_CARRIER_TUNE] = 57;
    start(&v,&p); render(&v,&p,a,N/2);
    p.value[DELXR_LXR_CARRIER_TUNE] = 81;
    render(&v,&p,a+N/2,N/2);
    const Metrics low = measure(a+4096), high = measure(a+12288);
    CHECK(high.centroid > low.centroid*2.5, "live two-octave tune edit must change pitch without retrigger");
}

static void test_attack_and_slope(void) {
    stage = "amplitude contour";
    DelxrLxrPatch p;
    isolated_tone(&p);
    p.value[DELXR_LXR_AMP_ATTACK] = 0; sound(&p,a);
    p.value[DELXR_LXR_AMP_ATTACK] = 112; sound(&p,b);
    CHECK(energy(b,512) < energy(a,512)*.25, "slow attack must suppress the initial hit");
    CHECK(energy(b+4096,4096) > energy(b,4096), "slow attack must rise after the onset");
    p.value[DELXR_LXR_AMP_ATTACK] = 0;
    p.value[DELXR_LXR_AMP_SLOPE] = 0; sound(&p,a);
    p.value[DELXR_LXR_AMP_SLOPE] = 120; sound(&p,b);
    CHECK(difference(a,b,N) > .01, "envelope slope must offer substantially different contours");
}

static void test_filters_and_transients(void) {
    stage = "filter and transient paths";
    DelxrLxrPatch p;
    defaults(&p);
    p.value[DELXR_LXR_CARRIER_WAVE] = 4;
    p.value[DELXR_LXR_CARRIER_TUNE] = 120;
    p.value[DELXR_LXR_MOD1_GAIN] = p.value[DELXR_LXR_MOD2_GAIN] = 0;
    knobs[2] = 1.f;
    for (int type = 0; type < 8; ++type) {
        p.value[DELXR_LXR_FILTER_TYPE] = type;
        for (int cutoff = 0; cutoff < 3; ++cutoff) for (int corner = 0; corner < 4; ++corner) {
            p.value[DELXR_LXR_FILTER_RESONANCE] = (corner & 1) ? 127 : 0;
            p.value[DELXR_LXR_FILTER_DRIVE] = (corner & 2) ? 127 : 0;
            p.value[DELXR_LXR_FILTER_CUTOFF] = cutoff == 0 ? 0 : cutoff == 1 ? 60 : 127;
            if (family == DELXR_LXR_FAMILY_SNARE) knobs[1] = cutoff*.5f;
            if (family >= DELXR_LXR_FAMILY_CYMBAL) knobs[3] = cutoff*.5f;
            sound(&p,a);
            if (cutoff == 1) CHECK(energy(a,N) > 100., "midrange filter must preserve audible output");
        }
    }
    if (family == DELXR_LXR_FAMILY_DRUM) {
        p.value[DELXR_LXR_FILTER_RESONANCE] = 0;
        p.value[DELXR_LXR_FILTER_DRIVE] = 40;
        p.value[DELXR_LXR_FILTER_CUTOFF] = 12;
        p.value[DELXR_LXR_FILTER_TYPE] = 0; sound(&p,a);
        p.value[DELXR_LXR_FILTER_TYPE] = 1; sound(&p,b);
        Metrics low = measure(a+4096), high = measure(b+4096);
        printf("  filter LP/HP spectral centroid %.0f / %.0f Hz\n",low.centroid,high.centroid);
        CHECK(high.centroid > low.centroid*1.5, "LP and HP must select different spectral regions");
    }
    isolated_tone(&p);
    p.value[DELXR_LXR_TRANSIENT_LEVEL] = 127;
    for (int wave = 0; wave < 14; ++wave) {
        p.value[DELXR_LXR_TRANSIENT_WAVE] = wave;
        for (int tuning = 0; tuning < 3; ++tuning) {
            p.value[DELXR_LXR_TRANSIENT_TUNE] = tuning == 0 ? 0 : tuning == 1 ? 64 : 127;
            sound(&p,a);
            CHECK(energy(a,N) > 100000., "transient selection must leave a sounding voice");
        }
    }
    p.value[DELXR_LXR_TRANSIENT_WAVE] = 11;
    p.value[DELXR_LXR_TRANSIENT_TUNE] = 25;
    p.value[DELXR_LXR_TRANSIENT_LEVEL] = 0; sound(&p,a);
    p.value[DELXR_LXR_TRANSIENT_LEVEL] = 127; sound(&p,b);
    CHECK(difference(a,b,2048) > .01, "PCM transient level must substantially reshape the attack");
}

static void test_modulators(void) {
    if (family == DELXR_LXR_FAMILY_SNARE) return;
    stage = "independent FM sound design";
    DelxrLxrPatch p;
    isolated_tone(&p);
    knobs[1] = .8f;
    p.value[DELXR_LXR_FM_MODE] = 0;
    p.value[DELXR_LXR_MOD1_GAIN] = 100;
    p.value[DELXR_LXR_MOD1_TUNE] = 57;
    p.value[DELXR_LXR_MOD2_GAIN] = 0;
    sound(&p,a);
    p.value[DELXR_LXR_MOD1_TUNE] = 93; sound(&p,b);
    CHECK(difference(a,b,N) > .05, "modulator tuning must substantially change FM timbre");
    for (int wave = 0; wave < 6; ++wave) {
        p.value[DELXR_LXR_MOD1_WAVE] = wave;
        sound(&p,waves[wave]);
        CHECK(energy(waves[wave],N) > 100000., "every FM modulator waveform must work");
    }
    CHECK(difference(waves[0],waves[4],N) > .05, "sine and noise modulation must have different textures");
    if (family == DELXR_LXR_FAMILY_DRUM) {
        p.value[DELXR_LXR_MOD1_WAVE] = 0;
        p.value[DELXR_LXR_FM_MODE] = 0; sound(&p,a);
        p.value[DELXR_LXR_FM_MODE] = 1; sound(&p,b);
        CHECK(difference(a,b,N) > .05, "mix mode must differ substantially from FM");
    } else {
        p.value[DELXR_LXR_MOD1_GAIN] = 0;
        p.value[DELXR_LXR_MOD2_GAIN] = 110;
        p.value[DELXR_LXR_MOD2_TUNE] = 50; sound(&p,a);
        p.value[DELXR_LXR_MOD2_TUNE] = 100; sound(&p,b);
        CHECK(difference(a,b,N) > .05, "second modulator must have independent tuning");
        for (int wave = 0; wave < 6; ++wave) {
            p.value[DELXR_LXR_MOD2_WAVE] = wave; sound(&p,waves[wave]);
        }
        CHECK(difference(waves[0],waves[4],N) > .05, "second modulator waveform must affect its own path");
        p.value[DELXR_LXR_MOD2_GAIN] = 0; sound(&p,a);
        p.value[DELXR_LXR_MOD2_GAIN] = 127; sound(&p,b);
        CHECK(difference(a,b,N) > .05, "second modulator gain must have useful range");
    }
}

static void test_snare_topology(void) {
    if (family != DELXR_LXR_FAMILY_SNARE) return;
    stage = "snare tonal/noise independence";
    DelxrLxrPatch p;
    isolated_tone(&p);
    p.value[DELXR_LXR_FILTER_TYPE] = 0; sound(&p,a);
    p.value[DELXR_LXR_FILTER_TYPE] = 1;
    p.value[DELXR_LXR_FILTER_RESONANCE] = 127;
    p.value[DELXR_LXR_NOISE_TUNE] = 0; sound(&p,b);
    CHECK(!memcmp(a,b,sizeof(a)), "noise filter/clock must not alter pure tonal snare");
    knobs[2] = 1.f;
    p.value[DELXR_LXR_FILTER_TYPE] = 7;
    p.value[DELXR_LXR_NOISE_TUNE] = 25; sound(&p,a);
    p.value[DELXR_LXR_NOISE_TUNE] = 124; sound(&p,b);
    Metrics slow = measure(a+4096), fast = measure(b+4096);
    printf("  snare noise-clock centroid %.0f / %.0f Hz\n",slow.centroid,fast.centroid);
    CHECK(fast.centroid > slow.centroid*2., "noise clock must span substantially different textures");
    knobs[2] = 0.f;
    p.value[DELXR_LXR_PITCH_AMOUNT] = 0; sound(&p,a);
    p.value[DELXR_LXR_PITCH_AMOUNT] = 127;
    p.value[DELXR_LXR_PITCH_DECAY] = 90; sound(&p,b);
    CHECK(difference(a,b,N) > .05, "snare pitch envelope must reshape its tonal body");
}

static void test_parameter_sweep(void) {
    stage = "all exposed parameter endpoints";
    DelxrLxrPatch p;
    for (int parameter = 0; parameter < DELXR_LXR_PATCH_PARAM_COUNT; ++parameter) {
        if (!delxr_lxr_patch_param_relevant(parameter,family)) continue;
        const int maximum = delxr_lxr_patch_param_max(parameter);
        const int values[] = {0, maximum/2, maximum, 255};
        for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
            defaults(&p);
            p.value[parameter] = values[i];
            sound(&p,a);
            CHECK(isfinite(energy(a,N)), "parameter endpoint must produce finite audio");
        }
    }
}

static void test_repeats(void) {
    if (family != DELXR_LXR_FAMILY_CYMBAL && family != DELXR_LXR_FAMILY_SNARE) return;
    stage = "repeat envelopes";
    DelxrLxrPatch p;
    isolated_tone(&p);
    p.value[DELXR_LXR_AMP_ATTACK] = 20;
    if (family == DELXR_LXR_FAMILY_SNARE) knobs[3] = 1.f;
    else p.value[DELXR_LXR_REPEAT] = 16;
    sound(&p,a);
    /* Repeated bursts must have alternating rises and falls before the main
     * tail. Count block-energy turns, excluding tiny numerical fluctuations. */
    unsigned rises = 0;
    double previous = energy(a,64);
    for (int offset = 64; offset < 8192; offset += 64) {
        double next = energy(a+offset,64);
        rises += next > previous*4. && next > 100000.;
        previous = next;
    }
    CHECK(rises >= 2, "repeat control must produce multiple distinct bursts");
    Voice v;
    start(&v,&p);
    int16_t block[257];
    int active = 1, samples = 0;
    while (active && samples < RATE*40) { active = render(&v,&p,block,257); samples += 257; }
    CHECK(!active, "repeat envelope must eventually complete");
    CHECK(!render(&v,&p,block,257) && energy(block,257) == 0., "completed repeated voice must remain silent");
}

static void test_bass_harmonics(void) {
    family = DELXR_LXR_FAMILY_DRUM;
    stage = "bass wavetable harmonic range";
    OscInfo bass = {0};
    bass.freq = 55.f;
    bass.pitchMod = bass.modNodeValue = 1.f;
    bass.waveform = SAW;
    osc_setFreq(&bass);
    /* Regression reference for the original integer-log underflow: every bass
     * note selected bank four. Verify the correction restores audible upper
     * partials, rather than merely asserting a private table index. */
    OscInfo oldSelection = bass;
    oldSelection.tableOffset = 4;
    for (int offset = 0; offset < N; offset += 64) {
        calcNextOscSampleBlock(&bass,a+offset,64,1.f,.5f);
        calcNextOscSampleBlock(&oldSelection,b+offset,64,1.f,.5f);
    }
    const Metrics corrected = measure(a+4096), old = measure(b+4096);
    printf("  55-Hz saw corrected/old-bank centroid %.1f/%.1f Hz, edge energy %.6f/%.6f\n",
           corrected.centroid,old.centroid,corrected.roughness,old.roughness);
    CHECK(corrected.centroid > old.centroid*1.05,"bass octave table must restore harmonic bandwidth");
    CHECK(corrected.roughness > old.roughness*1.2,"bass octave table must preserve sharper saw edges");
}

static void le32(FILE* f, uint32_t x) {
    for (int i = 0; i < 4; ++i) fputc((x >> (i*8)) & 255, f);
}

/* The runner translates the repository JSON fixture into a simple named-field
 * stream. Field lookup uses production metadata, so this test also catches a
 * misspelled or obsolete preset parameter instead of silently ignoring it. */
static void test_presets(const char* path) {
    stage = "eight authored JAM presets";
    FILE* input = fopen(path,"r");
    CHECK(input != NULL,"open generated preset fixtures");
    int count;
    CHECK(fscanf(input,"%d\n",&count) == 1 && count == 8,"expected eight authored presets");
    static int16_t presets[8][N];
    double minimumCentroid = RATE, maximumCentroid = 0.;
    FILE* wav = NULL;
    const char* preview = getenv("LXR_PREVIEW_WAV");
    enum { PREVIEW_SAMPLES = RATE*2 };
    if (preview && *preview) {
        wav = fopen(preview,"wb");
        CHECK(wav != NULL,"create requested preview WAV");
        const uint32_t bytes = count*PREVIEW_SAMPLES*2;
        fwrite("RIFF",1,4,wav); le32(wav,36+bytes); fwrite("WAVEfmt ",1,8,wav);
        le32(wav,16); fputc(1,wav); fputc(0,wav); fputc(1,wav); fputc(0,wav);
        le32(wav,RATE); le32(wav,RATE*2); fputc(2,wav); fputc(0,wav); fputc(16,wav); fputc(0,wav);
        fwrite("data",1,4,wav); le32(wav,bytes);
    }
    puts("  Authored preset                 RMS     centroid   edge energy");
    for (int i = 0; i < count; ++i) {
        char name[128];
        CHECK(fgets(name,sizeof(name),input) != NULL,"preset name");
        name[strcspn(name,"\r\n")] = 0;
        int transpose, fields;
        CHECK(fscanf(input,"%d %d",&family,&transpose) == 2 && family >= 0 && family <= 4,"preset family/pitch");
        DelxrLxrPatch p;
        defaults(&p);
        for (int k = 0; k < 4; ++k) CHECK(fscanf(input,"%f",&knobs[k]) == 1,"preset controls");
        pitchRatio = exp2f(transpose/12.f);
        CHECK(fscanf(input,"%d\n",&fields) == 1,"preset field count");
        for (int field = 0; field < fields; ++field) {
            char key[128]; int value, index;
            CHECK(fscanf(input,"%127s %d\n",key,&value) == 2,"preset field value");
            for (index = 0; index < DELXR_LXR_PATCH_PARAM_COUNT; ++index)
                if (!strcmp(key,delxr_lxr_patch_param_name(index))) break;
            CHECK(index < DELXR_LXR_PATCH_PARAM_COUNT,"preset parameter must exist");
            CHECK(value >= 0 && value <= delxr_lxr_patch_param_max(index),"preset parameter must be in range");
            p.value[index] = value;
        }
        sound(&p,presets[i]);
        Metrics m = measure(presets[i]);
        CHECK(m.rms > 100.,"authored preset must be audible");
        int clipping = 0;
        for (int sample = 0; sample < N; ++sample)
            clipping += abs(presets[i][sample]) >= 32760;
        CHECK(clipping < N/10,"authored preset must not depend on sustained hard clipping");
        for (int previous = 0; previous < i; ++previous)
            CHECK(difference(presets[i],presets[previous],N) > .05,"authored presets need substantive PCM differences");
        if (m.centroid < minimumCentroid) minimumCentroid = m.centroid;
        if (m.centroid > maximumCentroid) maximumCentroid = m.centroid;
        printf("  %-27s %7.0f %9.0f Hz %11.5f\n",name,m.rms,m.centroid,m.roughness);
        if (wav) {
            Voice v;
            start(&v,&p);
            for (int offset = 0; offset < PREVIEW_SAMPLES;) {
                int n = PREVIEW_SAMPLES-offset;
                if (n > N) n = N;
                render(&v,&p,a,n);
                for (int sample = 0; sample < n; ++sample) {
                    uint16_t value = (uint16_t)a[sample];
                    fputc(value & 255,wav); fputc(value >> 8,wav);
                }
                offset += n;
            }
        }
    }
    CHECK(maximumCentroid > minimumCentroid*4.,"preset library must cover genuinely different spectral regions");
    fclose(input);
    if (wav) { CHECK(!ferror(wav),"write complete preview WAV"); fclose(wav); }
}

int main(int argc, char** argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    puts("LXR sculptability: signal properties, topology, live edits and full DSP-path sweeps");
    test_bass_harmonics();
    for (family = DELXR_LXR_FAMILY_DRUM; family <= DELXR_LXR_FAMILY_OPEN_HAT; ++family) {
        test_waveforms();
        test_patch_isolation();
        test_attack_and_slope();
        test_filters_and_transients();
        test_modulators();
        test_snare_topology();
        test_repeats();
        test_parameter_sweep();
    }
    if (argc > 1) test_presets(argv[1]);
    printf("PASS: %u sculptability checks across five voice families\n",checks);
    return 0;
}
