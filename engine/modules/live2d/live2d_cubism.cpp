#include "live2d_cubism.h"
#include "engine/core/util/LogManager.h"
#include "engine/core/scene/NodeTree.h"
#include "render/renderer.h"
#include "render/Material.h"
#include "engine/core/script/lua/luaex.h"

// Ĭ�ϲ���
static const char* g_live2dDefaultMaterial = "\
<?xml version = \"1.0\" encoding = \"GB2312\"?>\
<material> \
<vs>#version 100\n\
\n\
attribute vec3 inPosition;\n\
attribute vec2 inTexCoord;\n\
\n\
uniform mat4 matWVP;\n\
\n\
varying vec2 texCoord;\n\
\n\
void main(void)\n\
{\n\
	vec4 position = matWVP * vec4(inPosition, 1.0);\n\
	gl_Position = position;\n\
	\n\
	texCoord = inTexCoord;\n\
}\n\
</vs>\
<ps>#version 100\n\
\n\
uniform sampler2D DiffuseSampler;\n\
varying mediump vec2 texCoord;\n\
\n\
void main(void)\n\
{\n\
	mediump vec4 textureColor = texture2D(DiffuseSampler, texCoord);\n\
	gl_FragColor = textureColor;\n\
}\n\
	</ps>\
	<BlendState>\
		<BlendEnable value = \"true\" />\
		<SrcBlend value = \"BF_SRC_ALPHA\" />\
		<DstBlend value = \"BF_INV_SRC_ALPHA\" />\
	</BlendState>\
	<RasterizerState>\
		<CullMode value = \"CULL_NONE\" />\
	</RasterizerState>\
	<DepthStencilState>\
		<DepthEnable value = \"false\" />\
		<WriteDepth value = \"false\" />\
	</DepthStencilState>\
	<SamplerState>\
		<BiLinearMirror>\
			<MinFilter value = \"FO_LINEAR\" />\
			<MagFilter value = \"FO_LINEAR\" />\
			<MipFilter value = \"FO_NONE\" />\
			<AddrUMode value = \"AM_CLAMP\" />\
			<AddrVMode value = \"AM_CLAMP\" />\
		</BiLinearMirror>\
	</SamplerState>\
	<Texture>\
		<stage no = \"0\" sampler = \"BiLinearMirror\" />\
	</Texture>\
	<VertexFormats>\
		<VertexFormat>\
			<VertexSemantic value = \"VS_POSITION\" />\
			<PixelFormat value = \"PF_RGB32_FLOAT\" />\
		</VertexFormat>\
		<VertexFormat>\
			<VertexSemantic value = \"VS_TEXCOORD\" />\
			<PixelFormat value = \"PF_RG32_FLOAT\" />\
		</VertexFormat>\
	</VertexFormats>\
</material>";

namespace Echo
{
	static void csmLogFunc(const char* message)
	{
		EchoLogError( message);
	}

	Live2dCubism::Live2dCubism()
		: m_moc(nullptr)
		, m_model(nullptr)
		, m_modelSize(0)
		, m_modelMemory(nullptr)
	{
		// set log fun
		csmSetLogFunction(csmLogFunc);

		setMoc("Res://girl/girl.moc3");
		buildRenderable();

		{
			//test
			luaex::LuaEx::instance()->loadfile("Res://lua/live2dcubism.lua", true);
			luaex::LuaEx::instance()->callf("start");
		}
	}

	Live2dCubism::~Live2dCubism()
	{
		EchoSafeFreeAlign(m_modelMemory, csmAlignofModel);
	}

	void Live2dCubism::bindMethods()
	{
		luaex::LuaEx::instance()->register_function<Live2dCubism>("Live2dCubism", "test", &Live2dCubism::test);

		CLASS_REGISTER_PROPERTY(Live2dCubism, "Moc", Variant::String, "getMoc", "setMoc");
		CLASS_REGISTER_PROPERTY(Live2dCubism, "Texture", Variant::String, "getPos", "setPos");
	}

	// parse paramters
	void Live2dCubism::parseParams()
	{
		int paramerCount = csmGetParameterCount(m_model);
		if (paramerCount > 0)
		{
			m_params.resize(paramerCount);
			const char** ids = csmGetParameterIds(m_model);
			const float* curValues = csmGetParameterValues(m_model);
			const float* defaultValues = csmGetParameterDefaultValues(m_model);
			const float* minValues = csmGetParameterMinimumValues(m_model);
			const float* maxValues = csmGetParameterMaximumValues(m_model);
			for (int i = 0; i < paramerCount; i++)
			{
				Paramter& param = m_params[i];
				param.m_name = ids[i];
				param.m_value = curValues[i];
				param.m_defaultValue = defaultValues[i];
				param.m_minValue = minValues[i];
				param.m_maxValue = maxValues[i];
			}
		}
	}

	// parse parts
	void Live2dCubism::parseParts()
	{
		int partCount = csmGetPartCount(m_model);
		if (partCount > 0)
		{
			m_parts.resize(partCount);
			const char** ids = csmGetPartIds(m_model);
			const float* opacities = csmGetPartOpacities(m_model);
			for (int i = 0; i < partCount; i++)
			{
				Part& part = m_parts[i];
				part.m_name = ids[i];
				part.m_opacities = opacities[i];
			}
		}
	}

	// parse drawables
	void Live2dCubism::parseDrawables()
	{
		m_localAABB.reset();

		int drawableCount = csmGetDrawableCount(m_model);
		if (drawableCount > 0)
		{
			m_drawables.resize(drawableCount);
			const char** ids = csmGetDrawableIds(m_model);
			const csmFlags* constantFlags = csmGetDrawableConstantFlags(m_model);
			const csmFlags* dynamicFlags = csmGetDrawableDynamicFlags(m_model);
			const int* textureIndices = csmGetDrawableTextureIndices(m_model);
			const int* drawOrders = csmGetDrawableDrawOrders(m_model);
			const int* renderOrders = csmGetDrawableRenderOrders(m_model);
			const float* opacities = csmGetDrawableOpacities(m_model);
			const int*  maskCounts = csmGetDrawableMaskCounts(m_model);
			const int** masks = csmGetDrawableMasks(m_model);
			const int* vertexCounts = csmGetDrawableVertexCounts( m_model);
			const csmVector2** positions = csmGetDrawableVertexPositions( m_model);
			const csmVector2** uvs = csmGetDrawableVertexUvs( m_model);
			const int* indexCounts = csmGetDrawableIndexCounts(m_model);
			const unsigned short** indices = csmGetDrawableIndices( m_model);
			for (int i = 0; i < drawableCount; i++)
			{
				// reference
				Drawable& drawable = m_drawables[i];

				drawable.m_name = ids[i];
				drawable.m_constantFlag = constantFlags[i];
				drawable.m_dynamicFlag = dynamicFlags[i];
				drawable.m_textureIndex = textureIndices[i];
				drawable.m_drawOrder = drawOrders[i];
				drawable.m_renderOrder = renderOrders[i];
				drawable.m_opacitie = opacities[i];
				ui32 maskCount = maskCounts[i];
				for (ui32 j = 0; j < maskCount; j++)
				{
					drawable.m_masks.push_back(masks[i][j]);
				}
				
				// vertexs
				ui32 vertexCount = vertexCounts[i];
				drawable.m_box.reset();
				for (ui32 j = 0; j < vertexCount; j++)
				{
					const csmVector2& pos = positions[i][j];
					const csmVector2& uv = uvs[i][j];

					VertexFormat vert;
					vert.m_position = Vector3(pos.X * m_canvas.m_pixelsPerUnit, pos.Y * m_canvas.m_pixelsPerUnit, 0.f);
					vert.m_uv = Vector2(uv.X, 1.f-uv.Y);

					drawable.m_vertices.push_back( vert);
					drawable.m_box.addPoint(vert.m_position);
				}

				// calc local aabb
				m_localAABB.unionBox(drawable.m_box);

				// indices
				ui32 indeceCount = indexCounts[i];
				for (ui32 j = 0; j < indeceCount; j++)
				{
					drawable.m_indices.push_back( indices[i][j]);
				}
			}

			std::sort(m_drawables.begin(), m_drawables.end(), [](const Drawable& a, const Drawable& b) ->int {return a.m_renderOrder<b.m_renderOrder; });
		}
	}

	// parse canvas info
	void Live2dCubism::parseCanvasInfo()
	{
		csmVector2 sizeInPixels;
		csmVector2 originInPixels;
		float	   pixelsPerUnit;
		csmReadCanvasInfo(m_model, &sizeInPixels, &originInPixels, &pixelsPerUnit);

		m_canvas.m_width = sizeInPixels.X;
		m_canvas.m_height = sizeInPixels.Y;
		m_canvas.m_originInPixels = Vector2(originInPixels.X, originInPixels.Y);
		m_canvas.m_pixelsPerUnit = pixelsPerUnit;
	}

	// set moc
	void Live2dCubism::setMoc(const String& res)
	{
		MemoryReaderAlign memReader( res, csmAlignofMoc);
		if (memReader.getSize())
		{
			m_moc = csmReviveMocInPlace(memReader.getData<void*>(), memReader.getSize());
			if ( m_moc)
			{
				m_modelSize = csmGetSizeofModel(m_moc);
				m_modelMemory = EchoMallocAlign(m_modelSize, csmAlignofModel);
				m_model = csmInitializeModelInPlace(m_moc, m_modelMemory, m_modelSize);

				parseCanvasInfo();
				parseParams();
				parseParts();
				parseDrawables();
			}
		}
	}

	// build drawable
	void Live2dCubism::buildRenderable()
	{
		VertexArray			vertices;
		vector<Word>::type	indices;
		for (Drawable& drawable : m_drawables)
		{
			ui32 vertOffset = vertices.size();

			// vertices
			for (VertexFormat& vert : drawable.m_vertices)
				vertices.push_back(vert);

			// indices
			for (int idx : drawable.m_indices)
				indices.push_back(idx + vertOffset);
		}

		Mesh::VertexDefine define;
		define.m_isUseDiffuseUV = true;

		m_mesh = Mesh::create(true, false);
		m_mesh->set(define, vertices.size(), (const Byte*)vertices.data(), indices.size(), indices.data(), m_localAABB);

		m_materialInst = MaterialInst::create();
		m_materialInst->setOfficialMaterialContent(g_live2dDefaultMaterial);
		m_materialInst->setRenderStage("Transparent");
		m_materialInst->applyLoadedData();
		m_materialInst->setTexture(0, "Res://girl/girl.1024/texture_00.png");

		m_renderable = Renderable::create(m_mesh, m_materialInst, this);
	}

	// update per frame
	void Live2dCubism::update()
	{
		if (m_model)
		{
			luaex::LuaEx::instance()->callf("update");

			static Matrix4 scale; scale.scaleReplace(Vector3(0.3f, 0.3f, 0.3f));

			m_matWVP = scale * NodeTree::instance()->get2DCamera()->getViewProjMatrix();;

			//csmUpdateModel((csmModel*)m_model);

			//updateVertexBuffer();

			m_renderable->submitToRenderQueue();
		}
	}

	// update vertex buffer
	void Live2dCubism::updateVertexBuffer()
	{
		int drawableCount = csmGetDrawableCount(m_model);
		if (drawableCount > 0)
		{
			if (m_drawables.size() == drawableCount)
			{
				const int* vertexCounts = csmGetDrawableVertexCounts(m_model);
				const csmVector2** positions = csmGetDrawableVertexPositions(m_model);
				const csmVector2** uvs = csmGetDrawableVertexUvs(m_model);

				for (int i = 0; i < drawableCount; i++)
				{
					// reference
					Drawable& drawable = m_drawables[i];
					drawable.m_vertices.clear();
					drawable.m_box.reset();

					// vertexs
					ui32 vertexCount = vertexCounts[i];
					drawable.m_box.reset();
					for (ui32 j = 0; j < vertexCount; j++)
					{
						const csmVector2& pos = positions[i][j];
						const csmVector2& uv = uvs[i][j];

						VertexFormat vert;
						vert.m_position = Vector3(pos.X, pos.Y, 0.f);
						vert.m_uv = Vector2(uv.X, 1.f-uv.Y);

						drawable.m_vertices.push_back(vert);
						drawable.m_box.addPoint(vert.m_position);
					}
				}
			}			
		}
	}

	// ��ȡȫ�ֱ���ֵ
	void* Live2dCubism::getGlobalUniformValue(const String& name)
	{
		if (name == "matWVP")
			return (void*)(&m_matWVP);

		return nullptr;
	}

	// update vertex buffer
	//void Live2dCubism::updateVertexBuffer()
	//{
	//	if (m_mesh)
	//	{
			//m_mesh->updateVertexs(m_vertices.size(), (const Byte*)m_vertices.data(), m_box);
	//	}
	//}
}