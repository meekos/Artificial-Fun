#include <iostream>
#include <cstdio>
#include "vjson/json.h"

class JsonDocument
{
private:
        block_allocator mAllocator;
        json_value *mRoot;

public:
        JsonDocument(): mAllocator(1 << 10), mRoot(0)
        {
        }

        bool parse(const char *filename)
        {
                FILE *fp = fopen(filename, "rb");
                if (!fp)
                {
                        printf("Can't open %s\n", filename);
                        return false;
                }

                fseek(fp, 0, SEEK_END);
                int size = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                char *source = (char *)mAllocator.malloc(size + 1);
                fread(source, 1, size, fp);
                source[size] = 0;

                fclose(fp);

                char *errorPos = 0;
                char *errorDesc = 0;
                int errorLine = 0;

                mRoot = json_parse(source, &errorPos, &errorDesc, &errorLine, &mAllocator);

                if (!mRoot)
                {
                        printf("Error at line %d: %s\n%s\n\n", errorLine, errorDesc, errorPos);
                        return false;
                }

                return true;
        }

        json_value *root()
        {
                return mRoot;
        }
};
 

int main() {

	// http://stackoverflow.com/questions/4273056/jsoncpp-not-reading-files-correctly
	// http://garajeando.blogspot.com/2011/06/how-to-install-jsoncpp-in-ubuntu-and.html
	// http://jsonrpc-cpp.sourceforge.net/index.php?n=Doc.FAQ

	JsonDocument();

    return 0;
}
