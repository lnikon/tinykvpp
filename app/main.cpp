#include <config/config.h>
#include <db/db.h>
#include <spdlog/common.h>

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

    spdlog::set_level(spdlog::level::debug);

    auto pConfig = config::make_shared();
    pConfig->LSMTreeConfig.DiskFlushThresholdSize = 128;
    db::db_t db(pConfig);
    if (!db.open())
    {
        std::cerr << "Unable to open the database" << std::endl;
        return 1;
    }

    // for (int i = 0; i < 3; i++)
    // {
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version1"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version2"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version3"});
    db.put(db::lsmtree::key_t{"cccccc"}, db::lsmtree::value_t{"aaaa"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version4"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version5"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version6"});
    db.put(db::lsmtree::key_t{"cccccc"}, db::lsmtree::value_t{"bbbb"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version7"});
    db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version8"});
    db.put(db::lsmtree::key_t{"ddddd"}, db::lsmtree::value_t{"version1"});
    db.put(db::lsmtree::key_t{"cccccc"}, db::lsmtree::value_t{"dddd"});
    //     db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version9"});
    //     db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version10"});
    //     db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version11"});
    //     db.put(db::lsmtree::key_t{"cccccc1"}, db::lsmtree::value_t{"aaaa1"});
    //     db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version12"});
    //     db.put(db::lsmtree::key_t{"aaaaaa2"}, db::lsmtree::value_t{"version13"});
    //     db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version13"});
    //     db.put(db::lsmtree::key_t{"cccccc"}, db::lsmtree::value_t{"bbbb2"});
    //     db.put(db::lsmtree::key_t{"aaaaaa"}, db::lsmtree::value_t{"version14"});
    //     db.put(db::lsmtree::key_t{"aaaaaa4"}, db::lsmtree::value_t{"version15"});
    //     db.put(db::lsmtree::key_t{"ddddd2"}, db::lsmtree::value_t{"version2"});
    //     db.put(db::lsmtree::key_t{"cccccc5"}, db::lsmtree::value_t{"dddd3"});
    // }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"aaaaaa"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"cccccc"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"ddddd"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"cccccc1"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"aaaaaa2"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"aaaaaa4"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"ddddd2"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"cccccc5"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    if (auto recordOpt{db.get(db::lsmtree::key_t{"ddddd"})}; recordOpt)
    {
        recordOpt->write(std::cout);
        std::cout << std::endl;
    }

    return 0;
}
