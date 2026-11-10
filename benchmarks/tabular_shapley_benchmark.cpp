//
//  tabular_shapley_benchmark.cpp
//  core-adapters
//
//  Copyright 2010-2026 Saxon Herschel Nicholls
//
//  Exact Shapley from a table of v(S) in C++: the baseline that tabular_shapley_benchmark.py compares the Python call
//  (snicholls_core.game.shapley(table)) with. The difference is the binding's call overhead: argument conversion, the
//  GIL round trip and building the result.
//
//  Both sides use the same table, v(S) = uniform_real(Wyrand(2026)) in coalition order, and the Python script refuses
//  to report anything unless the binding's values equal this program's bit for bit:
//
//      tabular_shapley_benchmark --values N     the Shapley values for n = N, as JSON (the fairness check)
//      tabular_shapley_benchmark [benchmark flags]
//

#include <snicholls/core/game/coalition.hpp>
#include <snicholls/core/game/shapley.hpp>
#include <snicholls/core/stats/random.hpp>

#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace game = snicholls::game;

namespace {

std::vector<double> make_table(std::size_t n)
{
    snicholls::stats::wyrand g(2026);
    std::vector<double> t(game::table_size(n));
    for (double& x : t)
        x = snicholls::stats::uniform_real(g);
    return t;
}

void BM_TabularShapley(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    const std::vector<double> t = make_table(n);
    const auto v = game::from_table(t);
    for (auto _ : state) {
        auto a = game::shapley(n, v);
        benchmark::DoNotOptimize(a.values.data());
        benchmark::ClobberMemory();
    }
    state.counters["coalitions"] = static_cast<double>(t.size());
}
BENCHMARK(BM_TabularShapley)->Arg(1)->Arg(4)->Arg(8)->Arg(12)->Arg(16)->Arg(20);

} // namespace

int main(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--values") == 0) {
        const auto n = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10));
        const std::vector<double> t = make_table(n);
        const auto a = game::shapley(n, game::from_table(t));
        std::printf("[");
        for (std::size_t i = 0; i < a.values.size(); ++i)
            std::printf("%s%.17g", i ? ", " : "", a.values[i]);
        std::printf("]\n");
        return 0;
    }
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
