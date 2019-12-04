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

int main(int argc, char *argv[])
{
    Endian = CheckEndian();
    
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <out> <file1> [<file2>...]\n", argv[0]);
        return 1;
    }
    int argi = 1;
    
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

            fprintf(out, "%u %u\n", R->timestamp, R->objectID);
        }

        fclose(f);
    }
    fclose(out);

    return 0;
}
