/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file src/roadveh.h Road vehicle states */

#ifndef ROADVEH_H
#define ROADVEH_H

#include "ground_vehicle.hpp"
#include "engine_base.h"
#include "cargotype.h"
#include "track_func.h"
#include "road.h"
#include "road_map.h"
#include "newgrf_engine.h"

struct RoadVehicle;

/** Road vehicle states */
enum RoadVehicleStates : uint8_t {
	/*
	 * Lower 4 bits are used for vehicle track direction. (Trackdirs)
	 * When in a road stop (bit 5 or bit 6 set) these bits give the
	 * track direction of the entry to the road stop.
	 * As the entry direction will always be a diagonal
	 * direction (X_NE, Y_SE, X_SW or Y_NW) only bits 0 and 3
	 * are needed to hold this direction. Bit 1 is then used to show
	 * that the vehicle is using the second road stop bay.
	 * Bit 2 is then used for drive-through stops to show the vehicle
	 * is stopping at this road stop.
	 */

	/* Numeric values */
	RVSB_IN_DEPOT                = 0xFE,                      ///< The vehicle is in a depot
	RVSB_WORMHOLE                = 0xFF,                      ///< The vehicle is in a tunnel and/or bridge

	/* Bit numbers */
	RVS_USING_SECOND_BAY         =    1,                      ///< Only used while in a road stop
	RVS_ENTERED_STOP             =    2,                      ///< Only set when a vehicle has entered the stop
	RVS_DRIVE_SIDE               =    4,                      ///< Only used when retrieving move data
	RVS_IN_ROAD_STOP             =    5,                      ///< The vehicle is in a road stop
	RVS_IN_DT_ROAD_STOP          =    6,                      ///< The vehicle is in a drive-through road stop

	/* Bit sets of the above specified bits */
	RVSB_IN_ROAD_STOP            = 1 << RVS_IN_ROAD_STOP,     ///< The vehicle is in a road stop
	RVSB_IN_ROAD_STOP_END        = RVSB_IN_ROAD_STOP + TRACKDIR_END,
	RVSB_IN_DT_ROAD_STOP         = 1 << RVS_IN_DT_ROAD_STOP,  ///< The vehicle is in a drive-through road stop
	RVSB_IN_DT_ROAD_STOP_END     = RVSB_IN_DT_ROAD_STOP + TRACKDIR_END,

	RVSB_DRIVE_SIDE              = 1 << RVS_DRIVE_SIDE,       ///< The vehicle is at the opposite side of the road

	RVSB_TRACKDIR_MASK           = 0x0F,                      ///< The mask used to extract track dirs
	RVSB_ROAD_STOP_TRACKDIR_MASK = 0x09,                      ///< Only bits 0 and 3 are used to encode the trackdir for road stops
};

/** State information about the Road Vehicle controller */
static const uint RDE_NEXT_TILE = 0x80; ///< We should enter the next tile
static const uint RDE_TURNED    = 0x40; ///< We just finished turning

/* Start frames for when a vehicle enters a tile/changes its state.
 * The start frame is different for vehicles that turned around or
 * are leaving the depot as the do not start at the edge of the tile.
 * For trams there are a few different start frames as there are two
 * places where trams can turn. */
static const uint RVC_DEFAULT_START_FRAME                =  0;
static const uint RVC_TURN_AROUND_START_FRAME            =  1;
static const uint RVC_DEPOT_START_FRAME                  =  6;
static const uint RVC_START_FRAME_AFTER_LONG_TRAM        = 21;
static const uint RVC_TURN_AROUND_START_FRAME_SHORT_TRAM = 16;
/* Stop frame for a vehicle in a drive-through stop */
static const uint RVC_DRIVE_THROUGH_STOP_FRAME           = 11;
static const uint RVC_DEPOT_STOP_FRAME                   = 11;

/** The number of ticks a vehicle has for overtaking. */
static const uint8_t RV_OVERTAKE_TIMEOUT = 35;

void RoadVehUpdateCache(RoadVehicle *v, bool same_length = false);
void GetRoadVehSpriteSize(EngineID engine, uint &width, uint &height, int &xoffs, int &yoffs, EngineImageType image_type);

/** Element of the RoadVehPathCache. */
struct RoadVehPathElement {
	Trackdir trackdir = INVALID_TRACKDIR; ///< Trackdir for this element.
	TileIndex tile = INVALID_TILE; ///< Tile for this element.

	constexpr RoadVehPathElement() {}
	constexpr RoadVehPathElement(Trackdir trackdir, TileIndex tile) : trackdir(trackdir), tile(tile) {}
};

using RoadVehPathCache = std::vector<RoadVehPathElement>;

/**
 * Buses, trucks and trams belong to this class.
 */
struct RoadVehicle final : public GroundVehicle<RoadVehicle, VEH_ROAD> {
	RoadVehPathCache path{};  ///< Cached path.
	uint8_t state = 0; ///< @see RoadVehicleStates
	uint8_t frame = 0;
	uint16_t blocked_ctr = 0;
	uint8_t overtaking = 0; ///< Set to #RVSB_DRIVE_SIDE when overtaking, otherwise 0.
	uint8_t overtaking_ctr = 0; ///< The length of the current overtake attempt.
	uint16_t crashed_ctr = 0; ///< Animation counter when the vehicle has crashed. @see RoadVehIsCrashed
	uint8_t reverse_ctr = 0;

	RoadType roadtype = INVALID_ROADTYPE; ///< NOSAVE: Roadtype of this vehicle.
	VehicleID disaster_vehicle = VehicleID::Invalid(); ///< NOSAVE: Disaster vehicle targetting this vehicle.
	RoadTypes compatible_roadtypes{}; ///< NOSAVE: Roadtypes this consist is powered on.

	/** We don't want GCC to zero our struct! It already is zeroed and has an index! */
	RoadVehicle() : GroundVehicleBase() {}
	/** We want to 'destruct' the right class. */
	virtual ~RoadVehicle() { this->PreDestructor(); }

	friend struct GroundVehicle<RoadVehicle, VEH_ROAD>; // GroundVehicle needs to use the acceleration functions defined at RoadVehicle.

	void MarkDirty() override;
	void UpdateDeltaXY() override;
	ExpensesType GetExpenseType(bool income) const override { return income ? EXPENSES_ROADVEH_REVENUE : EXPENSES_ROADVEH_RUN; }
	bool IsPrimaryVehicle() const override { return this->IsFrontEngine(); }
	void GetImage(Direction direction, EngineImageType image_type, VehicleSpriteSeq *result) const override;
	int GetDisplaySpeed() const override { return this->gcache.last_speed / 2; }
	int GetDisplayMaxSpeed() const override { return this->vcache.cached_max_speed / 2; }
	Money GetRunningCost() const override;
	int GetDisplayImageWidth(Point *offset = nullptr) const;
	bool IsInDepot() const override { return this->state == RVSB_IN_DEPOT; }
	bool Tick() override;
	void OnNewCalendarDay() override;
	void OnNewEconomyDay() override;
	uint Crash(bool flooded = false) override;
	Trackdir GetVehicleTrackdir() const override;
	TileIndex GetOrderStationLocation(StationID station) override;
	ClosestDepot FindClosestDepot() override;

	bool IsBus() const;

	int GetCurrentMaxSpeed() const override;
	int UpdateSpeed();
	void SetDestTile(TileIndex tile) override;

protected: // These functions should not be called outside acceleration code.

	/**
	 * Allows to know the power value that this vehicle will use.
	 * @return Power value from the engine in HP, or zero if the vehicle is not powered.
	 */
	inline uint16_t GetPower() const
	{
		/* Power is not added for articulated parts */
		if (!this->IsArticulatedPart()) {
			/* Road vehicle power is in units of 10 HP. */
			return 10 * GetVehicleProperty(this, PROP_ROADVEH_POWER, RoadVehInfo(this->engine_type)->power);
		}
		return 0;
	}

	/**
	 * Returns a value if this articulated part is powered.
	 * @return Zero, because road vehicles don't have powered parts.
	 */
	inline uint16_t GetPoweredPartPower(const RoadVehicle *) const
	{
		return 0;
	}

	/**
	 * Allows to know the weight value that this vehicle will use.
	 * @return Weight value from the engine in tonnes.
	 */
	inline uint16_t GetWeight() const
	{
		uint16_t weight = CargoSpec::Get(this->cargo_type)->WeightOfNUnits(this->cargo.StoredCount());

		/* Vehicle weight is not added for articulated parts. */
		if (!this->IsArticulatedPart()) {
			/* Road vehicle weight is in units of 1/4 t. */
			weight += GetVehicleProperty(this, PROP_ROADVEH_WEIGHT, RoadVehInfo(this->engine_type)->weight) / 4;
		}

		return weight;
	}

	/**
	 * Calculates the weight value that this vehicle will have when fully loaded with its current cargo.
	 * @return Weight value in tonnes.
	 */
	uint16_t GetMaxWeight() const override;

	/**
	 * Allows to know the tractive effort value that this vehicle will use.
	 * @return Tractive effort value from the engine.
	 */
	inline uint8_t GetTractiveEffort() const
	{
		/* The tractive effort coefficient is in units of 1/256.  */
		return GetVehicleProperty(this, PROP_ROADVEH_TRACTIVE_EFFORT, RoadVehInfo(this->engine_type)->tractive_effort);
	}

	/**
	 * Gets the area used for calculating air drag.
	 * @return Area of the engine in m^2.
	 */
	inline uint8_t GetAirDragArea() const
	{
		return 6;
	}

	/**
	 * Gets the air drag coefficient of this vehicle.
	 * @return Air drag value from the engine.
	 */
	inline uint8_t GetAirDrag() const
	{
		return RoadVehInfo(this->engine_type)->air_drag;
	}

	/**
	 * Checks the current acceleration status of this vehicle.
	 * @return Acceleration status.
	 */
	inline AccelStatus GetAccelerationStatus() const
	{
		return this->vehstatus.Test(VehState::Stopped) ? AS_BRAKE : AS_ACCEL;
	}

	/**
	 * Calculates the current speed of this vehicle.
	 * @return Current speed in km/h-ish.
	 */
	inline uint16_t GetCurrentSpeed() const
	{
		return this->cur_speed / 2;
	}

	/**
	 * Returns the rolling friction coefficient of this vehicle.
	 * @return Rolling friction coefficient in [1e-4].
	 */
	inline uint32_t GetRollingFriction() const
	{
		/* Trams have a slightly greater friction coefficient than trains.
		 * The rest of road vehicles have bigger values. */
		uint32_t coeff = RoadTypeIsTram(this->roadtype) ? 40 : 75;
		/* The friction coefficient increases with speed in a way that
		 * it doubles at 128 km/h, triples at 256 km/h and so on. */
		return coeff * (128 + this->GetCurrentSpeed()) / 128;
	}

	/**
	 * Allows to know the acceleration type of a vehicle.
	 * @return Zero, road vehicles always use a normal acceleration method.
	 */
	inline int GetAccelerationType() const
	{
		return 0;
	}

	/**
	 * Returns the slope steepness used by this vehicle.
	 * @return Slope steepness used by the vehicle.
	 */
	inline uint32_t GetSlopeSteepness() const
	{
		return _settings_game.vehicle.roadveh_slope_steepness;
	}

	/**
	 * Gets the maximum speed allowed by the track for this vehicle.
	 * @return Since roads don't limit road vehicle speed, it returns always zero.
	 */
	inline uint16_t GetMaxTrackSpeed() const
	{
		return GetRoadTypeInfo(GetRoadType(this->tile, GetRoadTramType(this->roadtype)))->max_speed;
	}

	/**
	 * Checks if the vehicle is at a tile that can be sloped.
	 * @return True if the tile can be sloped.
	 */
	inline bool TileMayHaveSlopedTrack() const
	{
		TrackStatus ts = GetTileTrackStatus(this->tile, TRANSPORT_ROAD, GetRoadTramType(this->roadtype));
		TrackBits trackbits = TrackStatusToTrackBits(ts);

		return trackbits == TRACK_BIT_X || trackbits == TRACK_BIT_Y;
	}

	/**
	 * Road vehicles have to use GetSlopePixelZ() to compute their height
	 * if they are reversing because in that case, their direction
	 * is not parallel with the road. It is safe to return \c true
	 * even if it is not reversing.
	 * @return are we (possibly) reversing?
	 */
	inline bool HasToUseGetSlopePixelZ()
	{
		const RoadVehicle *rv = this->First();

		/* Check if this vehicle is in the same direction as the road under.
		 * We already know it has either GVF_GOINGUP_BIT or GVF_GOINGDOWN_BIT set. */

		if (rv->state <= RVSB_TRACKDIR_MASK && IsReversingRoadTrackdir((Trackdir)rv->state)) {
			/* If the first vehicle is reversing, this vehicle may be reversing too
			 * (especially if this is the first, and maybe the only, vehicle).*/
			return true;
		}

		while (rv != this) {
			/* If any previous vehicle has different direction,
			 * we may be in the middle of reversing. */
			if (this->direction != rv->direction) return true;
			rv = rv->Next();
		}

		return false;
	}
};

#endif /* ROADVEH_H */
