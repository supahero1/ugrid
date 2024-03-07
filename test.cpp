#include "ugrid.hpp"

struct Entity : UGridEntity
{
};

#include <chrono>
#include <random>

std::mt19937 gen;

float
randf(
	float a,
	float b
	)
{
	return std::uniform_real_distribution<float>(a, b)(gen);
}

int
main(
	)
{
	std::random_device rd;
	gen = std::mt19937(rd());

	UGridCell GridCells = { 2048, 2048 };
	UGridDim CellDim = { 16.0f, 16.0f };
	UGrid<Entity> Grid(GridCells, CellDim);


	auto start = std::chrono::high_resolution_clock::now();

	for(uint32_t i = 0; i < 500000; ++i)
	{
		Entity Ent1;
		Ent1.Pos = { randf(0, GridCells.X * CellDim.W), randf(0, GridCells.Y * CellDim.H) };
		Ent1.Dim = { 7.0f, 7.0f };

		Grid.Insert(Ent1);
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << "Elapsed insertion time: " << duration.count() << " milliseconds" << std::endl;


	start = std::chrono::high_resolution_clock::now();

	Grid.Tick();

	end = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << "Elapsed tick time: " << duration.count() << " milliseconds" << std::endl;
}
