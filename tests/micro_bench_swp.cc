#include <iostream>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <thread>
#include <getopt.h>
#include <unistd.h>
#include <cmath>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <string.h>
#include <unordered_set>
#include "db_interface.h"
#include "timer.h"
#include "util.h"
#include "random.h"
#include "utils.h"

#define REST true
// #define REST false

using combotree::ComboTree;
using combotree::Random;
using combotree::Timer;
using ycsbc::KvDB;
using namespace dbInter;
using namespace std;

struct operation
{
    /* data */
    uint64_t op_type;
    uint64_t op_len; // for only scan
    uint64_t key_num;
};

int thread_num = 1;
size_t LOAD_SIZE = 2000000;
size_t PUT_SIZE = 10000000;
size_t GET_SIZE = 10000000;
int Loads_type = 3;
int Reverse = 0;

std::vector<uint64_t> data_base;
KvDB *db = nullptr;
Timer timer;
uint64_t us_times;
uint64_t load_pos = 0;
int load_size = 0;
size_t init_dram_space_use;
std::string dbName = "";

// 实时获取程序占用的内存，单位：kb。
size_t physical_memory_used_by_process()
{
    FILE *file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != nullptr)
    {
        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            int len = strlen(line);

            const char *p = line;
            for (; std::isdigit(*p) == false; ++p)
            {
            }

            line[len - 3] = 0;
            result = atoi(p);

            break;
        }
    }

    fclose(file);
    return result;
}

uint64_t get_longlat(const std::string &line)
{
    uint64_t id;
    double lat, lon;
    std::stringstream strin(line);
    strin >> id >> lon >> lat;
    return (lon * 180 + lat) * 1e7;
}

template <typename T>
std::vector<T> read_data_from_osm(
    const std::string load_file,
    T (*get_data)(const std::string &) = []
    { return static_cast<T>(0); },
    std::string output = "")
{
    std::vector<T> data;
    std::set<T> unique_keys;
    cout << "Use: " << __FUNCTION__ << endl;
    const uint64_t ns = util::timing(
        [&]
        {
            std::ifstream in(load_file);
            if (!in.is_open())
            {
                std::cerr << "unable to open " << load_file << endl;
                exit(EXIT_FAILURE);
            }
            uint64_t id, size = 0;
            double lat, lon;
            while (!in.eof())
            {
                /* code */
                std::string tmp;
                getline(in, tmp); // 去除第一行
                while (getline(in, tmp))
                {
                    T key = get_data(tmp);
                    unique_keys.insert(key);
                    size++;
                    if (size % 10000000 == 0)
                        std::cerr << "Load: " << size << "\r";
                }
            }
            in.close();
            std::cerr << "Finshed loads ......\n";
            data.assign(unique_keys.begin(), unique_keys.end());
            std::random_shuffle(data.begin(), data.end());
            size = data.size();
            std::cerr << "Finshed random ......\n";
            std::ofstream out(output, std::ios::binary);
            out.write(reinterpret_cast<char *>(&size), sizeof(uint64_t));
            out.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(uint64_t));
            out.close();
            cout << "read size: " << size << ", unique data: " << unique_keys.size() << endl;
        });
    const uint64_t ms = ns / 1e6;
    cout << "generate " << data.size() << " values in " << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms << " M values/s)" << endl;
    return data;
}

template <typename T>
std::vector<T> load_data_from_osm(
    const std::string dataname = "/home/lbl/dataset/generate_random_ycsb.dat")
{
    return util::load_data<T>(dataname, LOAD_SIZE, true);
}

std::vector<uint64_t> generate_random_ycsb(size_t op_num)
{
    std::vector<uint64_t> data;
    std::unordered_set<uint64_t> unique_keys;
    data.resize(op_num);
    cout << "Use: " << __FUNCTION__ << endl;
    const uint64_t ns = util::timing(
        [&]
        {
            Random rnd(0, UINT64_MAX);
            int cnt = 0;
            while (cnt < op_num)
            {
                int data = rnd.Next();
                ;
                if (unique_keys.insert(data).second)
                    cnt++;
                if (cnt % 10000000 == 0)
                    std::cerr << "generate: " << cnt << "\r";
            }
        });

    const std::string output = "/home/lbl/dataset/generate_random_ycsb.dat";
    data.assign(unique_keys.begin(), unique_keys.end());
    random_shuffle(data.begin(), data.end());
    std::ofstream out(output, std::ios::binary);
    uint64_t size = data.size();
    out.write(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    out.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(uint64_t));
    out.close();
    const uint64_t ms = ns / 1e6;
    cout << "generate " << data.size() << " values in " << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms << " M values/s)" << endl;
    for (int i = 0; i < 10; i++)
    {
        cout << i << ": " << data[i] << endl;
    }
    cout << "------------------------------" << endl;
    return data;
}

std::vector<uint64_t> generate_uniform_random(size_t op_num)
{
    std::vector<uint64_t> data;
    std::set<uint64_t> unique_keys;
    data.resize(op_num);
    cout << "Use: " << __FUNCTION__ << endl;
    const uint64_t ns = util::timing(
        [&]
        {
            Random rnd(0, UINT64_MAX);
            int cnt = 0;
            while (cnt < op_num)
            {
                int data = rnd.Next();
                ;
                if (unique_keys.count(data))
                    continue;
                else
                {
                    unique_keys.insert(data);
                    cnt++;
                }
            }
        });

    const std::string output = "/home/lbl/dataset/generate_uniform_random.dat";
    data.assign(unique_keys.begin(), unique_keys.end());
    random_shuffle(data.begin(), data.end());
    std::ofstream out(output, std::ios::binary);
    uint64_t size = data.size();
    out.write(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    out.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(uint64_t));
    out.close();
    const uint64_t ms = ns / 1e6;
    cout << "generate " << data.size() << " values in " << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms << " M values/s)" << endl;
    for (int i = 0; i < 10; i++)
    {
        cout << data[i] << endl;
    }
    return data;
}

void show_help(char *prog)
{
    cout << "Usage: " << prog << " [options]" << endl
         << endl
         << "  Option:" << endl
         << "    --thread[-t]             thread number" << endl
         << "    --load-size              LOAD_SIZE" << endl
         << "    --put-size               PUT_SIZE" << endl
         << "    --get-size               GET_SIZE" << endl
         //   << "    --workload               WorkLoad" << endl
         << "    --help[-h]               show help" << endl;
}

void remove_cache()
{
    int size = 256 * 1024 * 1024;
    char *garbage = new char[size];
    for (int i = 0; i < size; ++i)
        garbage[i] = i;
    for (int i = 100; i < size; ++i)
        garbage[i] += garbage[i - 100];
    delete[] garbage;
}

void load()
{
    cout << "Start loading ...." << endl;
    timer.Record("start");

    if (dbName == "alex" || dbName == "lipp" || dbName == "xindex" || dbName == "pgm" || dbName == "finedex") // support bulk load
    {
        auto values = new std::pair<uint64_t, uint64_t>[LOAD_SIZE];
        for (int i = 0; i < LOAD_SIZE; i++)
        {
            values[i].first = data_base[i];
            values[i].second = data_base[i] + 1;
        }
        sort(values, values + LOAD_SIZE,
             [](auto const &a, auto const &b)
             { return a.first < b.first; });

        db->Bulk_load(values, int(LOAD_SIZE));
    }
    else // put one by one
    {
        for (int i = 0; i < LOAD_SIZE; i++)
        {
            // cout << i << " put: " << data_base[i] << endl;
            db->Put(data_base[i], data_base[i] + 1);
        }
    }
    // std::cerr << endl;

    timer.Record("stop");
    us_times = timer.Microsecond("stop", "start");
    cout << "[Metic-Load]: Load " << LOAD_SIZE << ": "
         << "cost " << us_times / 1000000.0 << "s, "
         << "kops/s: " << (double)(LOAD_SIZE) / (double)us_times * 1000.0 << " ." << endl;
    cout << "after load, dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 / 1024.0 << " GB" << endl;
    load_pos = LOAD_SIZE;
    if (REST)
    {
        sleep(40);
    }
}

void test_uniform(string rwtype)
{
    if (REST)
    {
        sleep(60);
        remove_cache();
    }
    cout << "------------------------------" << endl;
    cout << "Start Testing Uniform Workload: ";
    size_t tot;
    if (rwtype == "r")
    {
        cout << "Read" << endl;
        tot = GET_SIZE;
    }
    else
    {
        cout << "Write" << endl;
        tot = PUT_SIZE;
    }
    util::FastRandom ranny(18);
    vector<uint32_t> rand_pos;
    std::mt19937_64 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dis(0, load_pos - 1);
    for (uint64_t i = 0; i < tot; i++)
    {
        rand_pos.push_back(ranny.RandUint32(0, load_pos - 1));
    }
    timer.Clear();
    timer.Record("start");

    int wrong_get = 0;
    uint64_t value = 0;
    if (rwtype == "r")
    {
        for (uint64_t i = 0; i < GET_SIZE; i++)
        {
            // cout << i << " get: " << data_base[rand_pos[i]] << endl;
            db->Get(data_base[rand_pos[i]], value);
            if (value != data_base[rand_pos[i]] + 1)
            {
                // cout << "wrong, value: " << value << ", suppose to be: " << data_base[rand_pos[i]] + 1 << endl;
                wrong_get++;
            }
        }
    }
    else
    {
        for (uint64_t i = 0; i < PUT_SIZE; i++)
        {
            // cout << "write: " << data_base[rand_pos[i]] << endl;
            db->Update(data_base[rand_pos[i]], ranny.RandUint32(0, INT32_MAX));
        }
    }

    timer.Record("stop");
    cout << "wrong get: " << wrong_get << endl;
    us_times = timer.Microsecond("stop", "start");
    cout << "[Metic-Operate]: Operate " << tot << ", "
         << "cost " << us_times / 1000000.0 << "s, "
         << "kops/s: " << (double)(tot) / (double)us_times * 1000.0 << " ." << endl;
    cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 / 1024.0 << " GB" << endl;
}

void test_all_zipfian()
{
    cout << "------------------------------" << endl;
    cout << "Start Testing Zipfian Workload" << endl;
    util::FastRandom ranny(18);
    vector<uint32_t> rand_pos;
    size_t tot = GET_SIZE + PUT_SIZE;
    std::vector<float> thetas = {0.6, 0.7, 0.8, 0.9, 0.95, 0.99};
    if (Reverse)
    {
        reverse(thetas.begin(), thetas.end());
    }
    float zipf_theta = 0.6;
    for (int k = 0; k < thetas.size(); k++)
    {
        if (REST)
        {
            sleep(60);
            remove_cache();
        }
        std::default_random_engine gen;
        zipfian_int_distribution<int> dis(0, load_pos - 1, thetas[k]);
        rand_pos.clear();
        for (uint64_t i = 0; i < tot; i++)
        {
            uint32_t pos = dis(gen);
            rand_pos.push_back(pos);
        }
        std::random_shuffle(rand_pos.begin(), rand_pos.end());

        timer.Clear();
        timer.Record("start");

        int wrong_get = 0;
        uint64_t value = 0;
        for (uint64_t i = 0; i < GET_SIZE; i++)
        {
            db->Get(data_base[rand_pos[i]], value);
            if (value != data_base[rand_pos[i]] + 1)
            {
                wrong_get++;
            }
        }
        for (uint64_t i = 0; i < PUT_SIZE; i++)
        {
            db->Put(data_base[rand_pos[i]], ranny.RandUint32(0, INT32_MAX));
        }

        timer.Record("stop");
        cout << "wrong get: " << wrong_get << endl;
        us_times = timer.Microsecond("stop", "start");
        cout << "[Metic-Operate]: Operate " << tot << " theta: " << thetas[k] << ", "
             << "cost " << us_times / 1000000.0 << "s, "
             << "kops/s: " << (double)(tot) / (double)us_times * 1000.0 << " ." << endl;
        cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 / 1024.0 << " GB" << endl;
    }
}

void init_opts(int argc, char *argv[])
{
    static struct option opts[] = {
        /* NAME               HAS_ARG            FLAG  SHORTNAME*/
        {"thread", required_argument, NULL, 't'},  // 0
        {"load-size", required_argument, NULL, 0}, // 1
        {"put-size", required_argument, NULL, 0},  // 2
        {"get-size", required_argument, NULL, 0},  // 3
        {"dbname", required_argument, NULL, 0},    // 4
        // {"workload", required_argument, NULL, 0},  // 5
        {"loadstype", required_argument, NULL, 0}, // 5
        {"reverse", required_argument, NULL, 0},   // 6
        {"help", no_argument, NULL, 'h'},          // 7
        {NULL, 0, NULL, 0}};

    int c;
    int opt_idx;
    std::string load_file = "";

    while ((c = getopt_long(argc, argv, "t:s:dh", opts, &opt_idx)) != -1)
    {
        switch (c)
        {
        case 0:
            switch (opt_idx)
            {
            case 0:
                thread_num = atoi(optarg);
                break;
            case 1:
                LOAD_SIZE = atoi(optarg);
                break;
            case 2:
                PUT_SIZE = atoi(optarg);
                break;
            case 3:
                GET_SIZE = atoi(optarg);
                break;
            case 4:
                dbName = optarg;
                break;
            case 5:
                Loads_type = atoi(optarg);
                break;
            case 6:
                Reverse = atoi(optarg);
                break;
            case 7:
                show_help(argv[0]);
                // return 0;
                exit(0);
            default:
                std::cerr << "Parse Argument Error!" << endl;
                abort();
            }
            break;
        case 't':
            thread_num = atoi(optarg);
            break;
        case 'h':
            show_help(argv[0]);
            // return 0;
            exit(0);
        case '?':
            break;
        default:
            cout << (char)c << endl;
            abort();
        }
    }

    cout << "THREAD NUMBER:         " << thread_num << endl;
    cout << "LOAD_SIZE:             " << LOAD_SIZE << endl;
    cout << "PUT_SIZE:              " << PUT_SIZE << endl;
    cout << "GET_SIZE:              " << GET_SIZE << endl;
    cout << "DB  name:              " << dbName << endl;
    cout << "Loads type:            " << Loads_type << endl;
    cout << "Reverse:               " << Reverse << endl;

    switch (Loads_type)
    {
    case 0:
        data_base = generate_uniform_random(LOAD_SIZE);
        break;
    case 1: // generate YCSB
        data_base = generate_random_ycsb(LOAD_SIZE);
        break;
    case 2:
        data_base = load_data_from_osm<uint64_t>("/home/lbl/dataset/generate_uniform_random.dat");
        break;
    case 3: // YCSB
        data_base = load_data_from_osm<uint64_t>("/home/lbl/dataset/generate_random_ycsb.dat");
        break;
    case 4: // LLT
        data_base = load_data_from_osm<uint64_t>("/home/lbl/dataset/generate_random_osm_longlat.dat");
        break;
    default:
        data_base = generate_uniform_random(LOAD_SIZE);
        break;
    }

    init_dram_space_use = physical_memory_used_by_process();
    cout << "before newdb, dram space use: " << init_dram_space_use / 1024.0 / 1024.0 << " GB" << endl;

    if (dbName == "fastfair")
    {
        db = new FastFairDb();
    }
    else if (dbName == "alex")
    {
        db = new AlexDB();
    }
    else if (dbName == "lipp")
    {
        db = new LippDB();
    }
    else if (dbName == "lbtree")
    {
        db = new LBTreeDB();
    }
    else if (dbName == "xindex")
    {
        db = new XIndexDB();
    }
    else if (dbName == "pgm")
    {
        db = new PGMDynamicDB();
    }
    else if (dbName == "finedex")
    {
        db = new FINEdexDB();
    }
    else
    {
        assert(false);
    }
}

int main(int argc, char *argv[])
{
    init_opts(argc, argv);
    db->Init();
    load();
    db->Info(); // print info such as index size
    test_uniform("r");
    if (dbName != "lipp") // LIPP provides no api for write
    {
        test_uniform("w");
    }
    return 0;
}
