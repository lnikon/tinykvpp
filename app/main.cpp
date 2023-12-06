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
int main(int argc, char *argv[]) {
  // TODO(lnikon): This is temp arg handling. Refactor.
  if (argc != 2) {
    spdlog::error("Usage: tkvp <path-to-db>");
    return 1;
  }

  auto pConfig = config::make_shared();
  db::db_t db(pConfig);

  return 0;
}
