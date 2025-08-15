#include "core/path_utils.h"
#include <stdio.h>
#include <string.h>

int rogue_find_asset_path(const char* filename, char* out, int out_sz){
        if(!filename||!out||out_sz<=0) return 0;
        const char* prefixes[] = { "assets/", "../assets/", "../../assets/", "../../../assets/" };
        char test[512];
        for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]); ++i){
                snprintf(test,sizeof test, "%s%s", prefixes[i], filename);
                FILE* f=NULL;
#if defined(_MSC_VER)
                fopen_s(&f,test,"rb");
#else
                f=fopen(test,"rb");
#endif
                if(f){ fclose(f);
#if defined(_MSC_VER)
                        strncpy_s(out,out_sz,test,_TRUNCATE);
#else
                        strncpy(out,test,out_sz-1); out[out_sz-1]='\0';
#endif
                        return 1; }
        }
        /* silent failure */
        return 0;
}
