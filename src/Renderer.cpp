
#include "SoftwareRenderer/Renderer.h"

#include <cmath>

#include <iostream> // DEBUG

namespace sr {

	// Private
	
	void Renderer::renderLine(const std::shared_ptr<pw::PixelWindow>& fb, int xBegin, int yBegin, int xEnd, int yEnd, int color) {
		int width = fb->getWidth();
		int height = fb->getHeight();
		
		int dx = abs(xEnd - xBegin);
		int sx = xBegin < xEnd ? 1 : -1;
		int dy = -std::abs(yEnd - yBegin);
		int sy = yBegin < yEnd ? 1 : -1;
		int err = dx + dy;
		int e2; /* error value e_xy */

		int x = xBegin;
		int y = yBegin;

		while (1) {
			if(x < width && x >= 0 && y < height && y >= 0) fb->setPixel(x, y, color);
			if (x == xEnd && y == yEnd) break;
			e2 = 2 * err;
			if (e2 > dy) {
				err += dy;
				x += sx;
			}
			if (e2 < dx) {
				err += dx;
				y += sy;
			}
		}
	}

	void Renderer::renderTriangleWireframe(const std::shared_ptr<pw::PixelWindow>& fb, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
		const int width = fb->getWidth();
		const int height = fb->getHeight();

		const auto pos1 = this->transformViewport(v1, width, height).getPosition();
		const auto pos2 = this->transformViewport(v2, width, height).getPosition();
		const auto pos3 = this->transformViewport(v3, width, height).getPosition();

		this->renderLine(fb, pos1.getX(), pos1.getY(), pos2.getX(), pos2.getY(), 0xFFFFFFFF);
		this->renderLine(fb, pos2.getX(), pos2.getY(), pos3.getX(), pos3.getY(), 0xFFFFFFFF);
		this->renderLine(fb, pos3.getX(), pos3.getY(), pos1.getX(), pos1.getY(), 0xFFFFFFFF);
	}

	void Renderer::renderTriangle(const std::shared_ptr<pw::PixelWindow>& fb, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
		const int width = fb->getWidth();
		const int height = fb->getHeight();
		
		const auto tv1 = this->transformViewport(v1, width, height);
		const auto tv2 = this->transformViewport(v2, width, height);
		const auto tv3 = this->transformViewport(v3, width, height);

		std::array<std::reference_wrapper<const Vertex>, 3> sorted = this->sortVerticesY(tv1, tv2, tv3);
		
		// generate 4th vertex
		float dy1 = sorted[0].get().getPosition().getY() - sorted[1].get().getPosition().getY();
		float dy2 = sorted[0].get().getPosition().getY() - sorted[2].get().getPosition().getY();

		if (dy2 == 0) {
			// Render flat top
			this->renderFlatTopTriangle(fb, sorted[0].get(), sorted[1].get(), sorted[2].get());
		}
		else {

			float alpha = dy1 / dy2;
			const auto tv4 = lerp(sorted[2].get(), sorted[0].get(), alpha);

			// sorted[1] to v4 is flat

			// render flat top
			this->renderFlatTopTriangle(fb, sorted[1].get(), tv4, sorted[2].get());

			// render flat bottom
			this->renderFlatBottomTriangle(fb, sorted[1].get(), tv4, sorted[0].get());

		}

	}

	void Renderer::renderFlatTopTriangle(const std::shared_ptr<pw::PixelWindow>& fb, const Vertex& v1, const Vertex& v2, const Vertex& v3) {

		float yBegin = std::ceilf(v3.getPosition().getY() - 0.5);
		float yEnd = std::ceilf(v1.getPosition().getY() - 0.5);

		float dy = (v1.getPosition().getY() - v3.getPosition().getY());

		float mxBegin = (v1.getPosition().getX() - v3.getPosition().getX()) / dy;
		float mxEnd = (v2.getPosition().getX() - v3.getPosition().getX()) / dy;


		for (float y = yBegin; y < yEnd; y += 1.0f) {
			// render row
			float xBegin = std::ceilf(v1.getPosition().getX() + (y - v1.getPosition().getY() + 0.5f) * mxBegin - 0.5f);
			float xEnd = std::ceilf(v2.getPosition().getX() + (y - v1.getPosition().getY() + 0.5f) * mxEnd - 0.5f);
			
			if (xEnd < xBegin) std::swap(xBegin, xEnd);
			
			this->renderLine(fb, v1, v2, v3, y, xBegin, xEnd);

		}

	}


	void Renderer::renderFlatBottomTriangle(const std::shared_ptr<pw::PixelWindow>& fb, const Vertex& v1, const Vertex& v2, const Vertex& v3) {

		float yBegin = std::ceilf(v1.getPosition().getY() - 0.5);
		float yEnd = std::ceilf(v3.getPosition().getY() - 0.5);

		float dy = (v1.getPosition().getY() - v3.getPosition().getY());

		float mxBegin = (v1.getPosition().getX() - v3.getPosition().getX()) / dy;
		float mxEnd = (v2.getPosition().getX() - v3.getPosition().getX()) / dy;


		for (float y = yBegin; y < yEnd; y += 1.0f) {
			// render row
			float xBegin = std::ceilf(v1.getPosition().getX() + (y - v1.getPosition().getY() + 0.5f) * mxBegin - 0.5f);
			float xEnd = std::ceilf(v2.getPosition().getX() + (y - v1.getPosition().getY() + 0.5f) * mxEnd - 0.5f);

			if (xEnd < xBegin) std::swap(xBegin, xEnd);

			this->renderLine(fb, v1, v2, v3, y, xBegin, xEnd);

		}
	}

	// try to vectorize later
	void Renderer::renderLine(const std::shared_ptr<pw::PixelWindow>& fb, const Vertex& v1, const Vertex& v2, const Vertex& v3, float y, float xBegin, float xEnd) {
		auto fs = this->fragmentShader.lock();
		if (fs == nullptr) return;

		for (float x = xBegin; x < xEnd; x += 1.0f) {
			if (x >= fb->getWidth() || x < 0 || y >= fb->getHeight() || y < 0) continue;
			
			Vertex interpolated = interpolateTriangle(v1, v2, v3, x, y);

			// invoke fragemntshader
			fs->in_color = interpolated.getColor();
			fs->in_position = interpolated.getPosition();
			fs->main();

			int color = this->convertColor(fs->out_color);
			
			fb->setPixel(int(x), int(y), color);

			fs->reset();
		}
	}

	Vertex Renderer::transformViewport(const Vertex& vert, int viewportWidth, int viewportHeight) const {
		Vertex out{};

		const auto& pos = vert.getPosition();
		const auto scale = 1.0f / pos.getW();

		out.position[0] = (pos.getX() * scale + 1.0f) * viewportWidth/2;
		out.position[1] = (-pos.getY() * scale + 1.0f) * viewportHeight/2;
		out.position[2] = pos.getZ() * scale;
		out.position[3] = pos.getW() * scale;

		out.color = vert.color;

		return out;
	}

	std::array<std::reference_wrapper<const Vertex>, 3> Renderer::sortVerticesY(const Vertex& v1, const Vertex& v2, const Vertex& v3) const {
		std::array<std::reference_wrapper<const Vertex>, 3> sorted = { v1,v2,v3 };

		if (sorted[0].get().getPosition().getY() < sorted[1].get().getPosition().getY()) std::swap(sorted[0], sorted[1]);
		if (sorted[0].get().getPosition().getY() < sorted[2].get().getPosition().getY()) std::swap(sorted[0], sorted[2]);
		if (sorted[1].get().getPosition().getY() < sorted[2].get().getPosition().getY()) std::swap(sorted[1], sorted[2]);

		return sorted;
	}

	Vertex Renderer::lerp(const Vertex& v1, const Vertex& v2, float alpha) const {
		Vertex out{};

		out.position = alpha * v1.getPosition() + (1 - alpha) * v2.getPosition();
		out.color = alpha * v1.getColor() + (1 - alpha) * v2.getColor();

		return out;
	}

	// Try to vectorize later
	Vertex Renderer::interpolateTriangle(const Vertex& v1, const Vertex& v2, const Vertex& v3, float x, float y) const {
		auto& p1 = v1.getPosition();
		auto& p2 = v2.getPosition();
		auto& p3 = v3.getPosition();
		
		float dy1 = p2.getY() - p3.getY();
		float dy2 = y - p3.getY();
		float dy3 = p3.getY() - p1.getY();
		float dy4 = -dy3;

		float dx1 = p3.getX() - p2.getX();
		float dx2 = x - p3.getX();
		float dx3 = p1.getX() - p3.getX();

		float denom = dy1 * dx3 + dx1 * dy4;

		// vectorize max?
		float w1 = std::max((dy1 * dx2 + dx1 * dy2) / denom, 0.0f);
		float w2 = std::max((dy3 * dx2 + dx3 * dy2) / denom, 0.0f);
		float w3 = std::max(1 - w1 - w2, 0.0f);

		Vertex v;

		v.position = w1 * v1.getPosition() + w2 * v2.getPosition() + w3 * v3.getPosition();
		v.color = w1 * v1.getColor() + w2 * v2.getColor() + w3 * v3.getColor();

		return v;

	}

	int Renderer::convertColor(const lm::Vector4f& color) const {
		auto c = 255.0f * color;

		int r = int(c.getX()) & 0xFF;
		int g = int(c.getY()) & 0xFF;
		int b = int(c.getZ()) & 0xFF;
		int a = int(c.getW()) & 0xFF;
		return a << 24 | b << 16 | g << 8 | r; // rbga -> abgr
	}

	std::pair<size_t, std::array<Vertex, 4>> Renderer::clipTriangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) const {
		const bool insideV1 = -v1.getPosition().getW() <= v1.getPosition().getZ();
		const bool insideV2 = -v2.getPosition().getW() <= v2.getPosition().getZ();
		const bool insideV3 = -v3.getPosition().getW() <= v3.getPosition().getZ();

		int inCount = insideV1 + insideV2 + insideV3;


		if(inCount == 3) return std::make_pair<size_t, std::array<Vertex, 4>>(1, {v1, v2, v3});
		if(inCount == 0) return std::make_pair<size_t, std::array<Vertex, 4>>(0, {});

		if (inCount == 1) {
			const Vertex& in = (insideV1 ? v1 : (insideV2 ? v2 : v3));
			const Vertex& out1 = (!insideV1 ? v1 : v2);
			const Vertex& out2 = (!insideV3 ? v3 : v2);
			
			auto near = (in.getPosition().getZ() - in.getPosition().getW()) / 2;

			auto alpha1 = -(in.getPosition().getW() + near) / (out1.getPosition().getW() - in.getPosition().getW());
			auto alpha2 = -(in.getPosition().getW() + near) / (out2.getPosition().getW() - in.getPosition().getW());

			auto v4 = this->lerp(out1, in, alpha1);
			auto v5 = this->lerp(out2, in, alpha2);

			return std::make_pair<size_t, std::array<Vertex, 4>>(1, {in, v4, v5});
		}

		// inCount == 2

		const Vertex& out = (!insideV1 ? v1 : (!insideV2 ? v2 : v3));
		const Vertex& in1 = (insideV1 ? v1 : v2);
		const Vertex& in2 = (insideV3 ? v3 : v2);

		auto near = (out.getPosition().getZ() - out.getPosition().getW()) / 2;

		auto alpha1 = -(in1.getPosition().getW() + near) / (out.getPosition().getW() - in1.getPosition().getW());
		auto alpha2 = -(in2.getPosition().getW() + near) / (out.getPosition().getW() - in2.getPosition().getW());

		auto v4 = this->lerp(out, in1, alpha1);
		auto v5 = this->lerp(out, in2, alpha2);

		return std::make_pair<size_t, std::array<Vertex, 4>>(2, { in1, in2, v5, v4 });
	}

	lm::Vector3f Renderer::getSurfaceNormal(const Vertex& v1, const Vertex& v2, const Vertex& v3) const {
		return lm::cross((v2.getPosition() - v1.getPosition()).getXYZ(), (v3.getPosition() - v1.getPosition()).getXYZ());
	}

	// Public

	void Renderer::setRenderSurface(std::weak_ptr<pw::PixelWindow> window) {
		this->frameBuffer = window;
	}

	void Renderer::enableBackfaceCulling() {
		this->backfaceCullingEnabled = true;
	}

	void Renderer::disableBackfaceCulling() {
		this->backfaceCullingEnabled = false;
	}

	void Renderer::bindFragmentShader(std::weak_ptr<FragmentShader> fs) {
		this->fragmentShader = fs;
	}

	void Renderer::renderLine(const Point2D& begin, const Point2D& end, int color) {
		auto fb = this->frameBuffer.lock();
		if (fb == nullptr) return;
		this->renderLine(fb, begin.getX(), begin.getY(), end.getX(), end.getY(), color);
	}

	void Renderer::renderTriangleWireframe(const Point2D& p1, const Point2D& p2, const Point2D& p3, int color) {
		auto fb = this->frameBuffer.lock();
		if (fb == nullptr) return;
		this->renderLine(fb, p1.getX(), p1.getY(), p2.getX(), p2.getY(), color);
		this->renderLine(fb, p2.getX(), p2.getY(), p3.getX(), p3.getY(), color);
		this->renderLine(fb, p3.getX(), p3.getY(), p1.getX(), p1.getY(), color);
	}

	void Renderer::renderTriangleWireframe(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
		auto fb = this->frameBuffer.lock();
		if (fb == nullptr) return;
		this->renderTriangleWireframe(fb, v1, v2, v3);
	}

	void Renderer::renderTriangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
		auto fb = this->frameBuffer.lock();
		if (fb == nullptr) return;
		this->renderTriangle(fb, v1, v2, v3);
	}

	void Renderer::render(RenderMode mode, const std::vector<Vertex>& vertices) {
		auto fb = this->frameBuffer.lock();
		if (fb == nullptr) return;

		if (mode == RenderMode::TRIANGLE) {
			// TODO Render Triangle (implemented later)
		}
		else if (mode == RenderMode::TRIANGLE_WIREFRAME) {
			// Render Wireframe
			// TODO
			for (size_t i = 0; i < vertices.size(); i += 3) {
				this->renderTriangleWireframe(fb, vertices[i], vertices[i + 1], vertices[i + 2]);
			}
		}
	}

	void Renderer::renderIndexed(RenderMode mode, const std::vector<Vertex>& vertices, const IntegerDataBuffer<3>& indices) {
		auto fb = this->frameBuffer.lock();
		if (fb == nullptr) return;

		if (mode == RenderMode::TRIANGLE) {
			for (size_t i = 0; i < indices.getAttributeCount(); ++i) {
				auto triangleIndices = indices.getVertexAttribute(i);

				const auto& v1 = vertices[triangleIndices[0]];
				const auto& v2 = vertices[triangleIndices[1]];
				const auto& v3 = vertices[triangleIndices[2]];

				auto surfaceNormal = -this->getSurfaceNormal(v1, v2, v3);

				// Backface culling
				auto pos = lm::Vector3f(v2.getPosition().getXY(), v2.getPosition().getW());
				auto dp = surfaceNormal * -pos;
				if (dp > 0 && backfaceCullingEnabled) continue;

				// Clipping
				auto clipped = this->clipTriangle(v1, v2, v3);
				if (clipped.first == 0) continue;
				const auto& verts = clipped.second;

				this->renderTriangle(fb, verts[0], verts[1], verts[2]);
				if (clipped.first == 2) this->renderTriangle(fb, verts[0], verts[2], verts[3]);

			}
		}
		else if (mode == RenderMode::TRIANGLE_WIREFRAME) {
			// Render Wireframe
			for (size_t i = 0; i < indices.getAttributeCount(); ++i) {
				auto triangleIndices = indices.getVertexAttribute(i);

				const auto& v1 = vertices[triangleIndices[0]];
				const auto& v2 = vertices[triangleIndices[1]];
				const auto& v3 = vertices[triangleIndices[2]];

				auto surfaceNormal = -this->getSurfaceNormal(v1, v2, v3);
				
				// Backface culling
				auto pos = lm::Vector3f(v2.getPosition().getXY(), v2.getPosition().getW());
				auto dp = surfaceNormal * -pos;
				if (dp > 0 && backfaceCullingEnabled) continue;

				// Clipping
				auto clipped = this->clipTriangle(v1, v2, v3);
				if (clipped.first == 0) continue;
				const auto& verts = clipped.second;

				this->renderTriangleWireframe(fb, verts[0], verts[1], verts[2]);
				if(clipped.first == 2) this->renderTriangleWireframe(fb, verts[0], verts[2], verts[3]);

			}
		}
	}

}
