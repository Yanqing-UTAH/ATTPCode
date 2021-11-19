#include <cstdio>
extern "C" {
#include <request.h>
#include <endian.h>
}
#include <cstdlib>
#include <unordered_map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <dirent.h>
#include <vector>
#include <string>
using namespace std;

int Endian = ITA_TOOL_NO_ENDIAN;

int main(int argc, char *argv[])
{
    Endian = CheckEndian();
    
    if (argc < 5)
    {
        fprintf(stderr, "usage: %s <outdir> <ncopies> <query_threshold> <path-to-decompressed-wc-logs>\n", argv[0]);
        return 1;
    }
    int argi = 1;
    
    std::string outdir = argv[argi++];
    int ncopies = atoi(argv[argi++]);
    std::string query_threshold_str = argv[argi++];
    std::string input_path = argv[argi++];

    std::vector<std::vector<std::string>> file_names;
    DIR *dp;
    dirent *ep;
    dp = opendir(input_path.c_str());
    if (!dp) {
        fprintf(stderr, "Error: failed to open directory %s\n", input_path.c_str());
        return 1;
    }
    while (ep = readdir(dp)) {
        if (ep->d_type != DT_REG) continue;
        std::string file_name = ep->d_name;
        if (file_name.substr(0, 6) == "wc_day") {
            auto p = file_name.find('_', 6);
            int day = atoi(file_name.substr(6, p - 6).c_str());
            int part = atoi(file_name.substr(p + 1, file_name.length() - p - 1).c_str());
            if (day >= file_names.size()) {
                file_names.resize(day + 1);
            }
            if (part >= file_names[day].size()) {
                file_names[day].resize(part + 1);
            }
            file_names[day][part] = file_name;
        }
    }

    uint64_t ts_offset = 0;
    uint64_t ts_delta = 0;
    uint64_t min_ts = 0;
    uint64_t max_ts = 0;
    int vday = 1;
    for (int copy_i = 0; copy_i < ncopies; ++copy_i)
    {
        for (int day = 5; day < 93; ++day)
        {
            FILE *out = fopen(
                (outdir + "/vday" + std::to_string(vday) + ".dat").c_str(),
                "w");
            for (int part = 1; part < file_names[day].size(); ++part)
            {
                struct request BER, LER, *R;
                
                std::string full_path = input_path + "/" + file_names[day][part];
                FILE *f = fopen(full_path.c_str(), "rb"); 
                if (!f)
                {
                    fprintf(stderr, "Error: Failed to read %s\n", full_path.c_str());
                    continue;
                }
                
                fprintf(stderr, "Processing %s\n", full_path.c_str());
                while (fread(&BER, sizeof(struct request), 1, f) == 1)
                {
                    switch (Endian)
                    {
                    case ITA_TOOL_LITTLE_ENDIAN:
                        LittleEndianRequest(&BER, &LER);
                        R = &LER;
                        break;
                    case ITA_TOOL_BIG_ENDIAN:
                        R = &BER;
                        break;
                    }
                    
                    if (min_ts == 0)
                    {
                        min_ts = R->timestamp;
                    }
                    max_ts = R->timestamp + ts_offset;
                    fprintf(out, "%lu %u\n", R->timestamp + ts_offset, R->clientID);
                }
                fclose(f);
            }
            fprintf(out, "+\n");
            fclose(out);

            out = fopen(
                (outdir + "/vday" + std::to_string(vday) + ".q").c_str(),
                "w");
            for (int i = 1; i <= 5; ++i) {
                fprintf(out, "%lu %s\n", 
                    (uint64_t)(min_ts + (max_ts - min_ts) * 1.0 * i / 5),
                    query_threshold_str.c_str());
            }
            fclose(out);

            ++vday;
        }
        if (copy_i == 0) {
            ts_delta = max_ts + 1 - min_ts;
        }
        ts_offset += ts_delta;
    }

    return 0;
}
