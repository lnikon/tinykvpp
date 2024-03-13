#include <config/config.h>
#include <db/db.h>

/**
 * TODO(lnikon): Add following arguments.
 * * db-path: string = "."
 * * segments-path: string = "segments"
 * * segments-size-mb: integer = "64mb"
 * TODO(lnikon): Support loading arguments from the command line.
 * TODO(lnikon): Support loading arguments from the JSON config file.
 */
int main(int argc, char *argv[])
{
    // TODO(lnikon): This is temp arg handling. Refactor.
    // if (argc != 2) {
    //   spdlog::error("Usage: tkvp <path-to-db>");
    //   return 1;
    // }

    auto pConfig = config::make_shared();
    pConfig->LSMTreeConfig.DiskFlushThresholdSize = 10;
    db::db_t db(pConfig);
    if (!db.open())
    {
        std::cerr << "Unable to open the database" << std::endl;
    }

    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version1"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version2"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version3"});
    db.put(db::lsmtree::key_t{"cccccc"}, db::lsmtree::value_t{"dddddd"});

    auto recordOpt{db.get(db::lsmtree::key_t{"aaaaaa"})};
    if (recordOpt)
    {
        const auto record{recordOpt.value()};
    }

    return 0;
}
