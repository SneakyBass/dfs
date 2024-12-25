#include <fmt/base.h>

#include "game.hh"
#include "injector.hh"
#include "network.hh"

int main(int, char *[]) {
    std::srand(std::time(NULL));

    dfs::GameData game_data{};
    if (!game_data.Initialize())
        return 1;

    dfs::Attach();

    dfs::Proxy proxy(5555, game_data);

    proxy.Run();

    return 0;
}
