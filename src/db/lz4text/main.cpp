#include "lz4text_db.hpp"

medusa::Database* GetDatabase(void)  { return new LZ4TextDatabase; }

int main(void) { return 0; }
