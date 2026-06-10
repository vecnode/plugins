#define PLUG_NAME "CamelotSynth"
#define PLUG_MFR "Vecnode"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'ghtf'
#define PLUG_MFR_ID 'Acme'
#define PLUG_URL_STR "https://iplug2.github.io"
#define PLUG_EMAIL_STR "spam@me.com"
#define PLUG_COPYRIGHT_STR "Copyright 2025 Acme Inc"
#define PLUG_CLASS_NAME CamelotSynth

#define BUNDLE_NAME "CamelotSynth"
#define BUNDLE_MFR "Vecnode"
#define BUNDLE_DOMAIN "com"

#define PLUG_CHANNEL_IO "0-2"
#define SHARED_RESOURCES_SUBPATH "CamelotSynth"

#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 480
#define PLUG_HEIGHT 280
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY CamelotSynth_Entry
#define AUV2_ENTRY_STR "CamelotSynth_Entry"
#define AUV2_FACTORY CamelotSynth_Factory
#define AUV2_VIEW_CLASS CamelotSynth_View
#define AUV2_VIEW_CLASS_STR "CamelotSynth_View"

#define AAX_TYPE_IDS 'IPI1', 'IPI2'
#define AAX_PLUG_MFR_STR "Acme"
#define AAX_PLUG_NAME_STR "CamelotSynth\nIPIS"
#define AAX_DOES_AUDIOSUITE 0
#define AAX_PLUG_CATEGORY_STR "Synth"

#define VST3_SUBCATEGORY "Instrument|Sampler"
#define CLAP_MANUAL_URL "https://iplug2.github.io/manuals/example_manual.pdf"
#define CLAP_SUPPORT_URL "https://github.com/iPlug2/wiki"
#define CLAP_DESCRIPTION "Embedded sample player"
#define CLAP_FEATURES "instrument", "sampler"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define ATMOS_SAMPLE_FN "AtmosSynth1_Dmaj.wav"
