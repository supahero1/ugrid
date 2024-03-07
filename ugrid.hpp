/*
 *   Copyright 2024 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <iostream>


struct UGridPos
{
	float X;
	float Y;
};

struct UGridDim
{
	float W;
	float H;
};

struct UGridCell
{
	uint32_t X;
	uint32_t Y;
};

struct UGridEntity
{
	UGridPos Pos;
	UGridDim Dim;
	uint32_t Copied = 0;
};

struct UGridReference
{
	uint32_t Next;
	uint32_t Ref;
};


template<typename T>
class UGridList
{
private:
	std::allocator<T> Allocator;
	T* List = nullptr;
	uint32_t Used = 1;
	uint32_t Size = 1;
	uint32_t Free = 0;
public:
	UGridList() = default;

	UGridList(
		const UGridList& Other
		)
	{
		this->Allocator = Other.Allocator;
		this->Size = std::min(Other.Size, Other.Used * 2);
		this->List = this->Allocator.allocate(this->Size);
	}

	UGridList<T>&
	operator=(
		UGridList&& Other
		)
	{
		if(this == &Other)
		{
			return *this;
		}

		this->Allocator.deallocate(this->List, this->Size);

		this->Allocator = Other.Allocator;
		this->List = Other.List;
		this->Used = Other.Used;
		this->Size = Other.Size;
		this->Free = Other.Free;

		Other.List = nullptr;

		return *this;
	}

	~UGridList(
		)
	{
		this->Allocator.deallocate(this->List, this->Size);
	}

	void
	SetAllocator(
		const std::allocator<T>& Allocator
		) noexcept
	{
		this->Allocator = Allocator;
	}

	T*
	GetPtr(
		)
	{
		return this->List;
	}

	void
	SetEnd(
		T* End
		)
	{
		this->Used = End - this->List;
	}

	uint32_t
	Get(
		)
	{
		if(this->Free)
		{
			uint32_t Ret = this->Free;
			this->Free = *reinterpret_cast<uint32_t*>(this->List + this->Free);
			return Ret;
		}

		if(this->Used == this->Size) [[unlikely]]
		{
			this->Size = (this->Size << 1) | 1;
			T* New = this->Allocator.allocate(this->Size);
			if(this->List)
			{
				memcpy(New, this->List, sizeof(*this->List) * this->Used);
				this->Allocator.deallocate(this->List, this->Size >> 1);
			}
			this->List = New;
		}

		return this->Used++;
	}

	void
	Ret(
		uint32_t Index
		) noexcept
	{
		*reinterpret_cast<uint32_t*>(this->List + Index) = this->Free;
		this->Free = Index;
	}

	T&
	operator[](
		std::size_t Index
		)
	{
		return this->List[Index];
	}
};


template<typename EntityType, typename = std::enable_if_t<std::is_base_of<UGridEntity, EntityType>::value>>
class UGrid
{
private:
	UGridList<EntityType> Entities;
	UGridList<UGridReference> References;

	std::allocator<uint32_t> CellAllocator;
	uint32_t* Cells;
	uint32_t* CellsEnd;

	UGridCell GridCells;
	UGridDim CellDim;
	UGridDim InverseCellDim;

	UGridCell
	PosToCell(
		UGridPos Pos
		)
	{
		uint32_t XCell = std::min(this->GridCells.X - 1,
			static_cast<uint32_t>(std::max(Pos.X, 0.0f) * this->InverseCellDim.W));
		uint32_t YCell = std::min(this->GridCells.Y - 1,
			static_cast<uint32_t>(std::max(Pos.Y, 0.0f) * this->InverseCellDim.H));
		return { XCell, YCell };
	}

	void
	Insert(
		uint32_t* Cell,
		uint32_t EntityIndex
		)
	{
		uint32_t Index = this->References.Get();
		this->References[Index].Next = *Cell;
		this->References[Index].Ref = EntityIndex;
		*Cell = Index;
	}

	void
	Optimize(
		)
	{
		UGridList<EntityType> NewEntities(this->Entities);
		EntityType* HeadEntity = NewEntities.GetPtr();
		EntityType* CurrentEntity = HeadEntity + 1;

		UGridList<UGridReference> NewReferences(this->References);
		UGridReference* HeadReference = NewReferences.GetPtr();
		UGridReference* CurrentReference = HeadReference + 1;

		for(uint32_t* Cell = this->Cells; Cell <= this->CellsEnd; ++Cell)
		{
			bool First = true;
			uint32_t i = *Cell;
			while(i)
			{
				UGridReference& Reference = this->References[i];
				i = Reference.Next;
				EntityType& Entity = this->Entities[Reference.Ref];

				if(!Entity.Copied)
				{
					*CurrentEntity = Entity;
					Entity.Copied = CurrentEntity - HeadEntity;
					++CurrentEntity;
				}

				if(First)
				{
					First = false;
					*Cell = CurrentReference - HeadReference;
				}

				UGridReference* NextReference = CurrentReference + 1;
				CurrentReference->Next = i ? NextReference - HeadReference : 0;
				CurrentReference->Ref = Entity.Copied;
				CurrentReference = NextReference;
			}
		}

		NewEntities.SetEnd(CurrentEntity);
		NewReferences.SetEnd(CurrentReference);

		this->Entities = std::move(NewEntities);
		this->References = std::move(NewReferences);
	}
public:
	UGrid(
		UGridCell GridCells,
		UGridDim CellDim
		)
	{
		this->GridCells = GridCells;
		this->CellDim = CellDim;

		uint32_t CellsNum = GridCells.X * GridCells.Y;

		this->InverseCellDim.W = 1.0f / CellDim.W;
		this->InverseCellDim.H = 1.0f / CellDim.H;

		this->Cells = this->CellAllocator.allocate(CellsNum);
		this->CellsEnd = this->Cells + CellsNum;
	}

	UGrid(
		UGridCell GridCells,
		UGridDim CellDim,
		const std::allocator<uint32_t>& CellAllocator
		)
	{
		this->CellAllocator = CellAllocator;
		this(GridCells, CellDim);
	}

	void
	SetEntityAllocator(
		const std::allocator<EntityType>& EntityAllocator
		) noexcept
	{
		this->Entities.SetAllocator(EntityAllocator);
	}

	void
	SetReferenceAllocator(
		const std::allocator<UGridReference>& ReferenceAllocator
		) noexcept
	{
		this->References.SetAllocator(ReferenceAllocator);
	}

	void
	Insert(
		EntityType Entity
		)
	{
		uint32_t Index = this->Entities.Get();
		this->Entities[Index] = Entity;

		UGridCell Start = this->PosToCell({ Entity.Pos.X - Entity.Dim.W, Entity.Pos.Y - Entity.Dim.H });
		UGridCell End = this->PosToCell({ Entity.Pos.X + Entity.Dim.W, Entity.Pos.Y + Entity.Dim.H });

		for(uint32_t X = Start.X; X <= End.X; ++X)
		{
			for(uint32_t Y = Start.Y; Y <= End.Y; ++Y)
			{
				this->Insert(this->Cells + X * this->GridCells.Y + Y, Index);
			}
		}
	}

	void
	Tick(
		)
	{
		this->Optimize();

		uint32_t GlobalMaxEntityIndex = 0;
		uint32_t Collisions = 0;

		for(uint32_t* Cell = this->Cells; Cell <= this->CellsEnd; ++Cell)
		{
			uint32_t LocalMaxEntityIndex = 0;

			uint32_t i = *Cell;
			while(i)
			{
				UGridReference& Reference = this->References[i];
				i = Reference.Next;
				LocalMaxEntityIndex = std::max(LocalMaxEntityIndex, Reference.Ref);

				if(Reference.Ref <= GlobalMaxEntityIndex) {
					continue;
				}

				// std::cout << "Update entity id " << Reference.Ref << std::endl;

				uint32_t j = i;
				while(j)
				{
					UGridReference& OtherReference = this->References[j];
					j = OtherReference.Next;

					if(OtherReference.Ref < GlobalMaxEntityIndex)
					{
						continue;
					}

					++Collisions;
					// std::cout << "Collision between entity id " << OtherReference.Ref << " and " << Reference.Ref << std::endl;
				}

			}

			GlobalMaxEntityIndex = std::max(GlobalMaxEntityIndex, LocalMaxEntityIndex);
		}

		std::cout << Collisions << " registered broad collisions" << std::endl;
	}
};
