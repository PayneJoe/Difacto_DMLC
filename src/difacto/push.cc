/*
 * \@file   push.cc
 * \brief   push checkpoint model into production
 *
 * Created by yuan pingzhou on 11/14/19.
 */

#include <iostream>
#include <thread>
#include <regex.h>
#include "base/match_file.h"
#include "dmlc/io.h"
#include "dmlc/logging.h"
#include "base/localizer.h"
using namespace dmlc;
typedef uint64_t K;

class PushWorker {
// value type stored on sever nodes, can be also other Entrys
struct AdaGradEntry {

    AdaGradEntry() { }
    ~AdaGradEntry() { Clear(); }

    inline void Clear() {
        if ( size > 1 ) { delete [] w; delete [] sqc_grad; }
        size = 0; w = NULL; sqc_grad = NULL;
    }

    /// length of w. if size == 1, then using w itself to store the value to save
    /// memory and avoid unnecessary new (see w_0())
    int size = 1;
    /// w and V
    float *w = NULL;
    /// square root of the cumulative gradient
    float *sqc_grad = NULL;

    inline float w_0() const { return size == 1 ? *(float *)&w : w[0]; }
    inline float sqc_grad_0() const {
        return size == 1 ? *(float *)&sqc_grad : sqc_grad[0];
    }

    void Load(Stream* fi) {
        fi->Read(&size, sizeof(size)) ;
        if (size == 1) {
            fi->Read(&w, sizeof(float*));
            fi->Read(&sqc_grad, sizeof(float*));
        } else {
            w = new float[size];
            sqc_grad = new float[size+1];
            fi->Read(w, sizeof(float)*size);
            fi->Read(sqc_grad, sizeof(float)*(size+1));
        }
    }

    bool Empty() const { return (w_0() == 0 && size == 1); }
};

public:
static void push(const std::string model_part_file, const std::string out_part_file, bool need_inverse, int thread_no) {
    Stream* fi = CHECK_NOTNULL(Stream::Create(model_part_file.c_str(), "r"));
    Stream* fo = CHECK_NOTNULL(Stream::Create(out_part_file.c_str(), "w"));
    int count = 0;
    while (true) {
        K key;
        AdaGradEntry age;
        if (fi->Read(&key, sizeof(K)) != sizeof(K)) break;
        age.Load(fi);
        uint64_t feature_id = need_inverse ? ReverseBytes(key) : key;
        std::string write_buffer = "";

        if (age.size == 1) {
            write_buffer = std::to_string(feature_id) + "\t" + std::to_string(age.w_0()) + "\n";
        } else {
            write_buffer.append(std::to_string(feature_id));
            for (int i = 0; i < age.size; ++i) {
                write_buffer.append("\t" + std::to_string(age.w[i]));
            }
            write_buffer.append("\n");
        }
        fo->Write((char*)write_buffer.c_str(), write_buffer.length());
        count += 1;
        if ((count > 0) && (count % 500 == 0)) {
            std::cout << "thread " << thread_no << " already pushed " << count << " kv pairs." << std::endl;
        }
    }
    delete fi; delete fo;
    std::cout << "thread " << thread_no << " total pushed " << count << " kv pairs." << std::endl;
}
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: <model_in> <push_out> [need_inverse]\n";
        return 0;
    }
    google::InitGoogleLogging(argv[0]);
    std::string model_in, push_out;
    bool need_inverse = false;
    for (int i = 1; i < argc; ++i) {
        char name[256], val[256];
        if (sscanf(argv[i], "%[^=]=%s", name, val) == 2) {
            if (!strcmp(name, "model_in")) model_in = val;
            if (!strcmp(name, "push_out")) push_out = val;
            if (!strcmp(name, "need_inverse")) need_inverse = !strcmp(val, "0") ? false : true;
        }
    }
    // match model parts
    std::vector<std::string> model_part_files;
    MatchFile(model_in, &model_part_files);
    if(model_part_files.size() == 0) {
        LOG(INFO) << "no matched model part files";
        return 1;
    }
    // push with multiple threads
    std::vector<std::thread> push_worker_threads;
    int n_thread = model_part_files.size();
    for (int i = 0;i < n_thread;i++) {
        size_t pos = model_part_files[i].find_last_of("/\\");
        std::string push_part_file = push_out + "/" + model_part_files[i].substr(pos + 1, model_part_files[i].length());
        std::thread worker(&PushWorker::push, model_part_files[i], push_part_file, need_inverse, i);
        push_worker_threads.push_back(std::move(worker));
    }
    std::for_each(push_worker_threads.begin(), push_worker_threads.end(), std::mem_fn(&std::thread::join));

    return 0;
}

