#pragma once

#include <LeptonMath/Vector.h>

namespace sr{

	using vec2 = lm::Vector2f;
	using vec3 = lm::Vector3f;
	using vec4 = lm::Vector4f;

	class FragmentShader {
		friend class Renderer;
	protected:

		// in
		vec4 in_position;
		vec4 in_color;

		// out
		vec4 out_color;

		virtual void main() = 0;

		void reset();

	public:
		virtual ~FragmentShader() = default;

	};

}