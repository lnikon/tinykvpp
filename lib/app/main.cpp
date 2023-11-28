#include <db/db.h>

int main(int argc, char *argv[]) {
  // TODO(lnikon): This is temp arg handling. Refactor.
  if (argc != 2) {
    spdlog::error("Usage: tkvp <path-to-db>");
    return 1;
  }

  db::db_t db(std::filesystem::path{argv[1]});

  return 0;
}
