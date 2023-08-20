// #pragma once
//
// // WIP
//
// #include "system_time.hpp"
// #include "io_types.hpp"
//
// namespace el1::wip::dev::sensor
// {
// 	using namespace io::types;
//
// 	struct ISensor;
//
// 	class TUnit
// 	{
// 		TString name;
// 		TString short_form;
// 	};
//
// 	class TMeasurand
// 	{
// 		TString name;
// 	};
//
// 	struct value_t
// 	{
// 		const TUnit* unit;
// 		fsys_t value;
// 	};
//
// 	struct metric_t
// 	{
// 		const TTime time;
// 		const TLocation* const location;
// 		const TMeasurand* const measurand;
// 		const ISensor* const sensor;
// 		const value_t value;
// 	};
//
// 	struct ISensor
// 	{
// 		virtual const TString& Name() const EL_GETTER = 0;
// 		virtual const TEvent<const metric_t&>& OnNewValue() const = 0;
// 	};
// }
