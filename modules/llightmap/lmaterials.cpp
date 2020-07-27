#include "lmaterials.h"

namespace LM
{

void LMaterial::Destroy()
{
	if (pAlbedo)
	{
		memdelete(pAlbedo);
		pAlbedo = 0;
	}
}

/////////////////////////////////

LMaterials::LMaterials()
{
}

LMaterials::~LMaterials()
{
	Reset();
}

void LMaterials::Reset()
{
	for (int n=0; n<m_Materials.size(); n++)
	{
		m_Materials[n].Destroy();
	}

	m_Materials.clear(true);
}


int LMaterials::FindOrCreateMaterial(const MeshInstance &mi, Ref<Mesh> rmesh, int surf_id)
{
	Ref<Material> src_material;

	// mesh instance has the material?
	src_material = mi.get_surface_material(surf_id);
	if (src_material.ptr())
	{
		//mi.set_surface_material(0, mat);
	}
	else
	{
		// mesh has the material?
		src_material = rmesh->surface_get_material(surf_id);
		//mi.set_surface_material(0, smat);
	}



//	Ref<Material> src_material = rmesh->surface_get_material(surf_id);
	const Material * pSrcMaterial = src_material.ptr();

	if (!pSrcMaterial)
		return 0;

	// already exists?
	for (int n=0; n<m_Materials.size(); n++)
	{
		if (m_Materials[n].pGodotMaterial == pSrcMaterial)
			return n+1;
	}

	// doesn't exist create a new material
	LMaterial * pNew = m_Materials.request();
	pNew->Create();
	pNew->pGodotMaterial = pSrcMaterial;

	// spatial material?
	Ref<SpatialMaterial> spatial_mat = src_material;
	Ref<Texture> albedo_tex;

	if (spatial_mat.is_valid())
	{
		albedo_tex = spatial_mat->get_texture(SpatialMaterial::TEXTURE_ALBEDO);
	}
	else
	{
		// shader material?
		Variant shader_tex = FindShaderTex(src_material);
		Ref<Texture> albedo_tex = shader_tex;
	} // not spatial mat

	Ref<Image> img_albedo;
	if (albedo_tex.is_valid())
	{
		img_albedo = albedo_tex->get_data();
		pNew->pAlbedo = _get_bake_texture(img_albedo, spatial_mat->get_albedo(), Color(0, 0, 0)); // albedo texture, color is multiplicative
		//albedo_texture = _get_bake_texture(img_albedo, size, mat->get_albedo(), Color(0, 0, 0)); // albedo texture, color is multiplicative
	} else
	{
		//albedo_texture = _get_bake_texture(img_albedo, size, Color(1, 1, 1), mat->get_albedo()); // no albedo texture, color is additive
	}


	// returns the new material ID plus 1
	return m_Materials.size();
}

Variant LMaterials::FindShaderTex(Ref<Material> src_material)
{

	Ref<ShaderMaterial> shader_mat = src_material;

	if (!shader_mat.is_valid())
		return Variant::NIL;

	// get the shader
	Ref<Shader> shader = shader_mat->get_shader();
	if (!shader.is_valid())
		return Variant::NIL;

	// find the most likely albedo texture
	List<PropertyInfo> plist;
	shader->get_param_list(&plist);

	String sz_first_obj_param;

	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		String szName = E->get().name;
		Variant::Type t = E->get().type;
//				print_line("shader param : " + szName);
//				print_line("shader type : " + String(Variant(t)));
		if (t == Variant::OBJECT)
		{
			sz_first_obj_param = szName;
			break;
		}

		//r_options->push_back(quote_style + E->get().name.replace_first("shader_param/", "") + quote_style);
	}

	if (sz_first_obj_param == "")
		return Variant::NIL;

	StringName pr = shader->remap_param(sz_first_obj_param);
	if (!pr) {
		String n = sz_first_obj_param;
		if (n.find("param/") == 0) { //backwards compatibility
			pr = n.substr(6, n.length());
		}
		if (n.find("shader_param/") == 0) { //backwards compatibility
			pr = n.replace_first("shader_param/", "");
		}
	}

	if (!pr)
		return Variant::NIL;

	Variant param = shader_mat->get_shader_param(pr);

	print_line("\tparam is " + String(param));
	return param;
}


LTexture * LMaterials::_get_bake_texture(Ref<Image> p_image, const Color &p_color_mul, const Color &p_color_add)
{
	LTexture * lt = memnew(LTexture);

	if (p_image.is_null() || p_image->empty())
	{
		// dummy texture
		lt->colors.resize(1);
		lt->colors.set(0, p_color_add);
		lt->width = 1;
		lt->height = 1;
		return lt;
	}

	int w = p_image->get_width();
	int h = p_image->get_height();
	int size = w * h;

	lt->width = w;
	lt->height = h;

	lt->colors.resize(size);

	PoolVector<uint8_t>::Read r = p_image->get_data().read();

	for (int i = 0; i<size; i++)
	{
		Color c;
		c.r = (r[i * 4 + 0] / 255.0) * p_color_mul.r + p_color_add.r;
		c.g = (r[i * 4 + 1] / 255.0) * p_color_mul.g + p_color_add.g;
		c.b = (r[i * 4 + 2] / 255.0) * p_color_mul.b + p_color_add.b;

		// srgb to linear?


		c.a = r[i * 4 + 3] / 255.0;

		lt->colors.set(i, c);
	}

	return lt;
}


bool LMaterials::FindColors(int mat_id, const Vector2 &uv, Color &aldedo)
{

	return true;
}


} // namespace
