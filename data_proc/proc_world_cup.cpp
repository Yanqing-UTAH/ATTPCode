#include <cstdio>
extern "C" {
#include <request.h>
#include <endian.h>
}
#include <cstdlib>
#include <unordered_map>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

int Endian = ITA_TOOL_NO_ENDIAN;

std::unordered_map<uint32_t, struct in_addr> ip_mapping;

#define RAND8() ((unsigned)(rand() & ((1 << 8) - 1)))
#define RAND32() ((RAND8() << 24) | (RAND8() << 16) | (RAND8() << 8) | RAND8())

const char *
map_client_id(uint32_t client_id)
{
    auto iter = ip_mapping.find(client_id);
    if (iter == ip_mapping.end())
    {
        struct in_addr n;
        n.s_addr = (in_addr_t) RAND32();
        ip_mapping[client_id] = n;
        return inet_ntoa(n);
    }

    return inet_ntoa(iter->second);
}

void read_ip_mapping(const char *name)
{
    FILE *f = fopen(name, "r");
    uint32_t client_id;
    char ip_str[17];
    struct in_addr n;

    if (!f) return ;
    while (fscanf(f, "%u %s\n", &client_id, ip_str) == 2)
    {
        if (!inet_aton(ip_str, &n))
        {
            fprintf(stderr, "Warning: invalid ip in mapping file %u %s\n", client_id, ip_str);
        }
        else
        {
            ip_mapping[client_id] = n;
        }
    }
    fclose(f);
}

void dump_ip_mapping(const char *name)
{
    FILE *f = fopen(name, "w");
    for (auto iter = ip_mapping.begin(); iter != ip_mapping.end(); ++iter)
    {
        fprintf(f, "%u %s\n", iter->first, inet_ntoa(iter->second));
    }
    fclose(f);
}

int main(int argc, char *argv[])
{
    Endian = CheckEndian();
    
    if (argc < 3 || (argv[1][0] == '-' && argc < 5))
    {
        fprintf(stderr, "usage: %s [-m <ip_mapping>] <out> <file1> [<file2>...]\n", argv[0]);
        return 1;
    }
    
    const char *ip_mapping_filename;
    int argi = 1;
    if (argv[1][0] == '-' )
    {
        if (argv[1][1] == 'm' && argv[1][2] == '\0')
        {
            ip_mapping_filename = argv[2];
            argi = 3;
        }
        else
        {
            fprintf(stderr, "Error: unknown option %s", argv[1]);
            return 1;
        }
    }
    ip_mapping_filename = "ip.map";
    read_ip_mapping(ip_mapping_filename);

    srand(19950810u);

    FILE *out = fopen(argv[argi++], "w");
    for (; argi < argc; ++argi)
    {
        struct request BER, LER, *R;

        FILE *f = fopen(argv[argi], "rb"); 
        if (!f)
        {
            fprintf(stderr, "Error: Failed to read %s\n", argv[argi]);
            continue;
        }

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

            fprintf(out, "%u %s\n", R->timestamp, map_client_id(R->clientID));
        }

        fclose(f);
    }
    fclose(out);

    dump_ip_mapping(ip_mapping_filename);
    
    return 0;
}
