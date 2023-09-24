#include "Transform.h"

#include <numbers>

namespace Registry
{
	Transform::Transform(const std::array<float, Offset::Total>& a_rawoffset) :
		_raw(a_rawoffset), _offset(a_rawoffset) {}

	Transform::Transform(std::ifstream& a_binarystream)
	{
		Decode::Read(a_binarystream, _raw[Offset::X]);
		Decode::Read(a_binarystream, _raw[Offset::Y]);
		Decode::Read(a_binarystream, _raw[Offset::Z]);
		Decode::Read(a_binarystream, _raw[Offset::R]);

    _offset = _raw;
	}

	const std::array<float, Offset::Total>& Transform::GetRawOffset() const
  {
    return _raw;
  }

	const std::array<float, Offset::Total>& Transform::GetOffset() const
  {
    return _offset;
  }

	void Transform::UpdateOffset(const std::array<float, Offset::Total>& a_newoffset)
	{
		_offset = a_newoffset;
	}

	void Transform::UpdateOffset(float a_value, Offset a_where)
	{
		_offset[a_where] += a_value;
	}

	void Transform::ResetOffset()
	{
		_offset = _raw;
	}

	void Transform::Apply(std::array<float, 4>& a_coordinate) const
	{
		const auto cos_theta = std::cosf(a_coordinate[3]);
		const auto sin_theta = std::sinf(a_coordinate[3]);

		a_coordinate[0] += (_offset[0] * cos_theta) - (_offset[1] * sin_theta);
		a_coordinate[1] += (_offset[0] * sin_theta) + (_offset[1] * cos_theta);
		a_coordinate[2] += _offset[2];
		if (_offset[3]) {
			a_coordinate[3] += _offset[3] * (std::numbers::pi_v<float> / 180.0f);
		}
	}

	void Transform::Save(YAML::Node& a_node) const
	{
		a_node = _offset;
	}

	void Transform::Load(const YAML::Node& a_node)
	{
		const auto offset = a_node.as<std::vector<float>>();
		for (size_t i = 0; i < Offset::Total; i++) {
			if (offset.size() >= i)
				break;
			_offset[i] = offset[i];
		}
	}

	bool Transform::HasChanges() const
	{
		return _offset != _raw;
	}

}
