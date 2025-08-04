// Real-time speech recognition of input from a microphone
//
// A very quick-n-dirty implementation serving mainly as a proof of concept.
//

#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2main.lib")

#ifdef _WIN32
#define NOMINMAX        // <-- 반드시 windows.h보다 먼저 선언
#include <windows.h>
#endif

#ifdef LL_USE_CUDA
#pragma comment(lib, "ggml-cuda.lib")
#pragma comment(lib, "CompilerIdCUDA.lib")
#pragma comment(lib, "cudart.lib")      // cudaDeviceSynchronize, cudaGetLastError ¡¦
#pragma comment(lib, "cuda.lib")        // cuDeviceGet, cuGetErrorString ¡¦
#pragma comment(lib, "cublas.lib")      // cublasCreate_v2, cublasDestroy_v2 ¡¦
#else
#pragma comment(lib, "cpulib/whisper.lib")   // build\lib\Release\whisper.lib
#pragma comment(lib, "cpulib/ggml.lib")      // build\lib\Release\ggml.lib
#pragma comment(lib, "cpulib/ggml-cpu.lib")
#pragma comment(lib, "cpulib/ggml-base.lib")
#endif


#include "common-sdl.h"
#include "system-audio.h"
#include "common.h"
#include "common-whisper.h"
#include "whisper.h"
#include "ggml-backend.h"
#include "vad.h"
#include "openai_client.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <memory>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

// Single-producer single-consumer ring buffer
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : m_buffer(capacity + 1), m_head(0), m_tail(0) {}

    bool push(T&& item) {
        size_t head = m_head.load(std::memory_order_relaxed);
        size_t next = (head + 1) % m_buffer.size();
        if (next == m_tail.load(std::memory_order_acquire)) {
            return false; // full
        }
        m_buffer[head] = std::move(item);
        m_head.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) {
            return false; // empty
        }
        item = std::move(m_buffer[tail]);
        m_tail.store((tail + 1) % m_buffer.size(), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
    }

private:
    std::vector<T> m_buffer;
    std::atomic<size_t> m_head;
    std::atomic<size_t> m_tail;
};

// command-line parameters
struct whisper_params {
    int32_t n_threads = std::thread::hardware_concurrency();//std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t step_ms = 500;//500;
    int32_t length_ms  = 6000;
    int32_t keep_ms    = 1000;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;
    int32_t beam_size  = -1;

    float vad_thold    = 0.6f;
    float freq_thold   = 100.0f;

    bool translate     = false;
    bool no_fallback   = false;
    bool print_special = false;
    bool no_context    = true;
    bool no_timestamps = false;
    bool tinydiarize   = false;
    bool save_audio    = false; // save audio to wav file
#ifdef LL_USE_CUDA
    bool use_gpu       = true;
#else
    bool use_gpu       = false;
#endif
    bool flash_attn    = true;
    bool use_openai    = false;

    std::string language  = "ko";

    std::string model = "models/ggml-tiny.bin";
    //std::string model = "models/ggml-small.bin";
    //std::string model = "models/ggml-base.bin";
    //std::string model = "models/ggml-medium.bin";
    //std::string model = "models/ggml-large-v3-turbo-q5_0.bin";
    //std::string model = "models/ggml-large-v3-turbo.bin";
    //std::string model = "models/ggml-large-v3-turbo-q8_0.bin";
    std::string fname_out;
};

void whisper_print_usage(int argc, char ** argv, const whisper_params & params);

static bool whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-t"    || arg == "--threads")       { params.n_threads     = std::stoi(argv[++i]); }
        else if (                  arg == "--step")          { params.step_ms       = std::stoi(argv[++i]); }
        else if (                  arg == "--length")        { params.length_ms     = std::stoi(argv[++i]); }
        else if (                  arg == "--keep")          { params.keep_ms       = std::stoi(argv[++i]); }
        else if (arg == "-c"    || arg == "--capture")       { params.capture_id    = std::stoi(argv[++i]); }
        else if (arg == "-mt"   || arg == "--max-tokens")    { params.max_tokens    = std::stoi(argv[++i]); }
        else if (arg == "-ac"   || arg == "--audio-ctx")     { params.audio_ctx     = std::stoi(argv[++i]); }
        else if (arg == "-bs"   || arg == "--beam-size")     { params.beam_size     = std::stoi(argv[++i]); }
        else if (arg == "-vth"  || arg == "--vad-thold")     { params.vad_thold     = std::stof(argv[++i]); }
        else if (arg == "-fth"  || arg == "--freq-thold")    { params.freq_thold    = std::stof(argv[++i]); }
        else if (arg == "-tr"   || arg == "--translate")     { params.translate     = true; }
        else if (arg == "-nf"   || arg == "--no-fallback")   { params.no_fallback   = true; }
        else if (arg == "-ps"   || arg == "--print-special") { params.print_special = true; }
        else if (arg == "-kc"   || arg == "--keep-context")  { params.no_context    = false; }
        else if (arg == "-l"    || arg == "--language")      { params.language      = argv[++i]; }
        else if (arg == "-m"    || arg == "--model")         { params.model         = argv[++i]; }
        else if (arg == "-f"    || arg == "--file")          { params.fname_out     = argv[++i]; }
        else if (arg == "-tdrz" || arg == "--tinydiarize")   { params.tinydiarize   = true; }
        else if (arg == "-sa"   || arg == "--save-audio")    { params.save_audio    = true; }
        else if (arg == "-ng"   || arg == "--no-gpu")        { params.use_gpu       = false; }
        else if (arg == "-fa"   || arg == "--flash-attn")    { params.flash_attn    = true; }

        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

void whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,       --help          [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,     --threads N     [%-7d] number of threads to use during computation\n",    params.n_threads);
    fprintf(stderr, "            --step N        [%-7d] audio step size in milliseconds\n",                params.step_ms);
    fprintf(stderr, "            --length N      [%-7d] audio length in milliseconds\n",                   params.length_ms);
    fprintf(stderr, "            --keep N        [%-7d] audio to keep from previous step in ms\n",         params.keep_ms);
    fprintf(stderr, "  -c ID,    --capture ID    [%-7d] capture device ID\n",                              params.capture_id);
    fprintf(stderr, "  -mt N,    --max-tokens N  [%-7d] maximum number of tokens per audio chunk\n",       params.max_tokens);
    fprintf(stderr, "  -ac N,    --audio-ctx N   [%-7d] audio context size (0 - all)\n",                   params.audio_ctx);
    fprintf(stderr, "  -bs N,    --beam-size N   [%-7d] beam size for beam search\n",                      params.beam_size);
    fprintf(stderr, "  -vth N,   --vad-thold N   [%-7.2f] voice activity detection threshold\n",           params.vad_thold);
    fprintf(stderr, "  -fth N,   --freq-thold N  [%-7.2f] high-pass frequency cutoff\n",                   params.freq_thold);
    fprintf(stderr, "  -tr,      --translate     [%-7s] translate from source language to english\n",      params.translate ? "true" : "false");
    fprintf(stderr, "  -nf,      --no-fallback   [%-7s] do not use temperature fallback while decoding\n", params.no_fallback ? "true" : "false");
    fprintf(stderr, "  -ps,      --print-special [%-7s] print special tokens\n",                           params.print_special ? "true" : "false");
    fprintf(stderr, "  -kc,      --keep-context  [%-7s] keep context between audio chunks\n",              params.no_context ? "false" : "true");
    fprintf(stderr, "  -l LANG,  --language LANG [%-7s] spoken language\n",                                params.language.c_str());
    fprintf(stderr, "  -m FNAME, --model FNAME   [%-7s] model path\n",                                     params.model.c_str());
    fprintf(stderr, "  -f FNAME, --file FNAME    [%-7s] text output file name\n",                          params.fname_out.c_str());
    fprintf(stderr, "  -tdrz,    --tinydiarize   [%-7s] enable tinydiarize (requires a tdrz model)\n",     params.tinydiarize ? "true" : "false");
    fprintf(stderr, "  -sa,      --save-audio    [%-7s] save the recorded audio to a file\n",              params.save_audio ? "true" : "false");
    fprintf(stderr, "  -ng,      --no-gpu        [%-7s] disable GPU inference\n",                          params.use_gpu ? "false" : "true");
    fprintf(stderr, "  -fa,      --flash-attn    [%-7s] flash attention during inference\n",               params.flash_attn ? "true" : "false");
    fprintf(stderr, "\n");
}

int main(int argc, char ** argv) {
    std::setlocale(LC_ALL, ".65001");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    ggml_backend_load_all();

    whisper_params params;

    if (whisper_params_parse(argc, argv, params) == false) {
        return 1;
    }

    //params.keep_ms   = std::min(params.keep_ms,   params.step_ms);
    params.length_ms = std::max(params.length_ms, params.step_ms);

    const int n_samples_step = (1e-3*params.step_ms  )*WHISPER_SAMPLE_RATE;
    const int n_samples_len  = (1e-3*params.length_ms)*WHISPER_SAMPLE_RATE;
    const int n_samples_keep = (1e-3*params.keep_ms  )*WHISPER_SAMPLE_RATE;
    const int n_samples_30s  = (1e-3*30000.0         )*WHISPER_SAMPLE_RATE;

    const bool use_vad = n_samples_step <= 0; // sliding window mode uses VAD

    const int n_new_line = !use_vad ? std::max(1, params.length_ms / params.step_ms - 1) : 1; // number of steps to print new line

    params.no_timestamps  = !use_vad;
    params.no_context    |= use_vad;
    params.max_tokens     = 0;

    // select and init audio source
    std::cout << "Select input source (0: microphone, 1: system audio): ";
    int audio_choice = 0;
    std::cin >> audio_choice;

    std::unique_ptr<audio_capture> audio;
    if (audio_choice == 1) {
        audio = std::make_unique<system_audio_async>(params.length_ms);
    } else {
        audio = std::make_unique<audio_async>(params.length_ms);
    }

    if (!audio->init(params.capture_id, WHISPER_SAMPLE_RATE)) {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio->resume();

    std::cout << "Select inference engine (0: local model, 1: OpenAI API): ";
    int engine_choice = 0;
    std::cin >> engine_choice;
    params.use_openai = (engine_choice == 1);

    // whisper init
    if (params.language != "auto" && whisper_lang_id(params.language.c_str()) == -1){
        fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    struct whisper_context_params cparams = whisper_context_default_params();

    cparams.use_gpu    = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    struct whisper_context * ctx = nullptr;
    if (!params.use_openai) {
        ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);
        if (ctx == nullptr) {
            fprintf(stderr, "error: failed to initialize whisper context\n");
            return 2;
        }
    }

    std::vector<float> pcmf32_new(n_samples_30s, 0.0f);

    std::vector<whisper_token> prompt_tokens;

    // print some info about the processing
    {
        fprintf(stderr, "\n");
        if (!params.use_openai && ctx && !whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        fprintf(stderr, "%s: processing %d samples (step = %.1f sec / len = %.1f sec / keep = %.1f sec), %d threads, lang = %s, task = %s, timestamps = %d ...\n",
                __func__,
                n_samples_step,
                float(n_samples_step)/WHISPER_SAMPLE_RATE,
                float(n_samples_len )/WHISPER_SAMPLE_RATE,
                float(n_samples_keep)/WHISPER_SAMPLE_RATE,
                params.n_threads,
                params.language.c_str(),
                params.translate ? "translate" : "transcribe",
                params.no_timestamps ? 0 : 1);

        if (!use_vad) {
            fprintf(stderr, "%s: n_new_line = %d, no_context = %d\n", __func__, n_new_line, params.no_context);
        } else {
            fprintf(stderr, "%s: using VAD, will transcribe on speech activity\n", __func__);
        }

        fprintf(stderr, "\n");
    }

    int n_iter = 0;

    std::atomic<bool> is_running(true);
    std::ofstream log_file("transcription.log", std::ios::app);

    std::ofstream fout;
    if (params.fname_out.length() > 0) {
        fout.open(params.fname_out, std::ios::app);
        if (!fout.is_open()) {
            fprintf(stderr, "%s: failed to open output file '%s'!\n", __func__, params.fname_out.c_str());
            return 1;
        }
    }

    set_log_files(nullptr, fout.is_open() ? &fout : nullptr);
    
    wav_writer wavWriter;
    if (params.save_audio) {
        time_t now = time(0);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", localtime(&now));
        std::string filename = std::string(buffer) + ".wav";

        wavWriter.open(filename, WHISPER_SAMPLE_RATE, 16, 1);
    }

    RingBuffer<std::vector<float>> audio_queue(8);
    std::thread inference_thread([&]() {
        std::vector<float> pcmf32(n_samples_30s, 0.0f);
        std::vector<float> pcmf32_old;
        std::vector<float> pcmf32_new_local;
        std::string sentence;
        int n_iter = 0;
        while (is_running.load()) {
            /*if (!audio_queue.pop(pcmf32_new_local)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }*/
                        // 최소 한 청크는 받아오기
            if (!audio_queue.pop(pcmf32_new_local)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;

            }
            // 비어있는 버퍼가 들어오면 현재까지의 텍스트를 종료하고 컨텍스트를 초기화
            if (pcmf32_new_local.empty()) {
                printf("\n");
                pcmf32_old.clear();
                if (!params.no_context) {
                    prompt_tokens.clear();
                }
                n_iter = 0;
                continue;
            }
            // 큐에 남은 모든 청크를 한데 모아서 pcmf32_new_local에 합치기
            std::vector<float> backlog;
            while (audio_queue.pop(backlog)) {
                pcmf32_new_local.insert(
                    pcmf32_new_local.end(),
                    backlog.begin(), backlog.end()
                     );
                
            }
            if (params.save_audio) {
                wavWriter.write(pcmf32_new_local.data(), pcmf32_new_local.size());
            }
            const int n_samples_new = pcmf32_new_local.size();
            const int n_samples_take = std::min((int) pcmf32_old.size(), std::max(0, n_samples_keep + n_samples_len - n_samples_new));
            pcmf32.resize(n_samples_new + n_samples_take);
            for (int i = 0; i < n_samples_take; i++) {
                pcmf32[i] = pcmf32_old[pcmf32_old.size() - n_samples_take + i];
            }
            memcpy(pcmf32.data() + n_samples_take, pcmf32_new_local.data(), n_samples_new*sizeof(float));
            pcmf32_old = pcmf32;

            if (params.use_openai) {
                std::string text = openai_transcribe(pcmf32, params.language);
                if (!text.empty()) {
                    timestamped_print("%s", text.c_str());
                    sentence = text;
                }
            } else {
                whisper_full_params wparams = whisper_full_default_params(params.beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY);
                wparams.print_progress   = false;
                wparams.print_special    = params.print_special;
                wparams.print_realtime   = false;
                wparams.print_timestamps = !params.no_timestamps;
                wparams.translate        = params.translate;
                wparams.single_segment   = !use_vad;
                wparams.max_tokens       = params.max_tokens;
                wparams.language         = params.language.c_str();
                wparams.n_threads        = params.n_threads;
                wparams.beam_search.beam_size = params.beam_size;
                wparams.audio_ctx        = params.audio_ctx;
                wparams.tdrz_enable      = params.tinydiarize;
                wparams.temperature_inc  = params.no_fallback ? 0.0f : wparams.temperature_inc;
                wparams.prompt_tokens    = params.no_context ? nullptr : prompt_tokens.data();
                wparams.prompt_n_tokens  = params.no_context ? 0       : prompt_tokens.size();

                if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
                    fprintf(stderr, "%s: failed to process audio\n", argv[0]);
                    is_running.store(false);
                    break;
                }

                if (!use_vad) {
                    printf("\33[2K\r");
                    printf("%s", std::string(100, ' ').c_str());
                    printf("\33[2K\r");
                }
                const int n_segments = whisper_full_n_segments(ctx);
                for (int i = 0; i < n_segments; ++i) {
                    const char * text = whisper_full_get_segment_text(ctx, i);

                    if (params.no_timestamps) {
                        timestamped_print("%s", text);
                    } else {
                        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
                        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

                        std::string output = "[" + to_timestamp(t0, false) + " --> " + to_timestamp(t1, false) + "]  " + text;

                        timestamped_print("%s", output.c_str());
                    }
                    sentence = text;
                    //sentence += " ";
                }
            }

            ++n_iter;
            if ((n_iter % n_new_line) == 0) {
                printf("\n");
                if (log_file.is_open()) {
                    time_t nowt = time(nullptr);
                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&nowt));
                    log_file << "[" << buf << "] " << sentence << std::endl;
                }
                sentence.clear();
                pcmf32_old = std::vector<float>(pcmf32.end() - n_samples_keep, pcmf32.end());
                if (!params.use_openai && !params.no_context) {
                    prompt_tokens.clear();
                    const int n_segments = whisper_full_n_segments(ctx);
                    for (int i = 0; i < n_segments; ++i) {
                        const int token_count = whisper_full_n_tokens(ctx, i);
                        for (int j = 0; j < token_count; ++j) {
                            prompt_tokens.push_back(whisper_full_get_token_id(ctx, i, j));
                        }
                    }
                }
            }
            fflush(stdout);
        }
    });

    timestamped_print("[Start speaking]\n");

    auto last_voice_time = std::chrono::steady_clock::now();
    bool sent_silence = false;
    const int silence_timeout_ms = 2000;

    // main audio loop
    while (is_running.load()) {
        // handle Ctrl + C
        if (!sdl_poll_events()) {
            is_running.store(false);
            break;
        }

        while (is_running.load()) {
            audio->get(params.step_ms, pcmf32_new);

            /*if ((int)pcmf32_new.size() > 2 * n_samples_step) {
                fprintf(stderr, "\n\n%s: WARNING: cannot process audio fast enough, dropping audio ...\n\n", __func__);
                audio->clear();
                continue;
            }*/
            if ((int)pcmf32_new.size() > 2 * n_samples_step) {
                fprintf(stderr,
                    "\n\n%s: WARNING: capture backlog size = %zu samples (not dropping)\n\n",
                    __func__, pcmf32_new.size());// 더 이상 드롭하지 않고, backlog 처리에 맡김
            }

            if ((int) pcmf32_new.size() >= n_samples_step) {
                audio->clear();
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (!is_running.load()) {
            break;
        }

        // Skip sending audio to the model if no speech is detected
        bool is_speech = vad_detect_speech(pcmf32_new, WHISPER_SAMPLE_RATE);
        if (!is_speech) {
            auto now = std::chrono::steady_clock::now();
            if (!sent_silence &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_voice_time).count() > silence_timeout_ms) {
                while (!audio_queue.push(std::vector<float>()) && is_running.load()) {
                    std::vector<float> drop;
                    audio_queue.pop(drop);
                }
                sent_silence = true;
            }
            continue;
        }

        last_voice_time = std::chrono::steady_clock::now();
        sent_silence = false;

        while (!audio_queue.push(std::move(pcmf32_new)) && is_running.load()) {
            std::vector<float> drop;
            audio_queue.pop(drop);
        }
    }

    is_running.store(false);
    inference_thread.join();

    audio->pause();

    if (ctx) {
        whisper_print_timings(ctx);
        whisper_free(ctx);
    }

    return 0;
}

