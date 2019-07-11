#include "smooth.h"
#include "core/engine.h"


#define SMOOTHCLASS Smooth
#define SMOOTHNODE Spatial
#include "smooth_body.inl"

// finish the bind with custom stuff
BIND_ENUM_CONSTANT(METHOD_SLERP);
BIND_ENUM_CONSTANT(METHOD_LERP);
//ClassDB::bind_method(D_METHOD("set_lerp"), &SMOOTHCLASS::set_lerp);
//ClassDB::bind_method(D_METHOD("get_lerp"), &SMOOTHCLASS::get_lerp);
ClassDB::bind_method(D_METHOD("set_method", "method"), &SMOOTHCLASS::set_method);
ClassDB::bind_method(D_METHOD("get_method"), &SMOOTHCLASS::get_method);

ADD_GROUP("Misc", "");
//ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lerp"), "set_lerp", "get_lerp");
ADD_PROPERTY(PropertyInfo(Variant::INT, "method", PROPERTY_HINT_ENUM, "Slerp,Lerp"), "set_method", "get_method");
}

#undef SMOOTHCLASS
#undef SMOOTHNODE

Smooth::Smooth() {
	//count = 0;
	m_Flags = 0;
	SetFlags(SF_ENABLED | SF_TRANSLATE | SF_ROTATE);
//	m_Mode = MODE_AUTO;

//	m_bEnabled = true;
//	m_bInterpolate_Rotation = true;
//	m_bInterpolate_Scale = false;
//	m_bLerp = false;
	//m_ptCurr.zero();
	//m_ptPrev.zero();
//	m_bDirty = false;
//	m_fFraction = 0.0f;
}

void Smooth::set_method(eMethod p_method)
{
	ChangeFlags(SF_LERP, p_method == METHOD_LERP);
}

Smooth::eMethod Smooth::get_method() const
{
	if (TestFlags(SF_LERP))
		return METHOD_LERP;

	return METHOD_SLERP;
}


//void Smooth::set_lerp(bool bLerp)
//{
//	ChangeFlags(SF_LERP, bLerp);
//}

//bool Smooth::get_lerp() const
//{
//	return TestFlags(SF_LERP);
//}


void Smooth::teleport()
{
	Spatial * pTarget = GetTarget();
	if (!pTarget)
	return;

	RefreshTransform(pTarget);

	// set previous equal to current
	m_Prev = m_Curr;
	m_ptTranslateDiff.zero();

}

void Smooth::RefreshTransform(Spatial * pTarget, bool bDebug)
{
	ClearFlags(SF_DIRTY);

	// keep the data flowing...
	// translate..
	m_Prev.m_Transform.origin = m_Curr.m_Transform.origin;

	// global or local?
	bool bGlobal = TestFlags(SF_GLOBAL_IN);

	// new transform for this tick
	Transform trans;
	if (bGlobal)
		trans = pTarget->get_global_transform();
	else
		trans = pTarget->get_transform();

	//const Transform &trans = pTarget->get_transform();

	m_Curr.m_Transform.origin = trans.origin;
	m_ptTranslateDiff = m_Curr.m_Transform.origin - m_Prev.m_Transform.origin;


	// lerp? keep the basis
	if (TestFlags(SF_LERP))
	{
		m_Prev.m_Transform.basis = m_Curr.m_Transform.basis;
		m_Curr.m_Transform.basis = trans.basis;
	}
	else
	{
		if (TestFlags(SF_ROTATE))
		{
			m_Prev.m_qtRotate = m_Curr.m_qtRotate;
			m_Curr.m_qtRotate = trans.basis.get_rotation_quat();
		}

		if (TestFlags(SF_SCALE))
		{
			m_Prev.m_ptScale = m_Curr.m_ptScale;
			m_Curr.m_ptScale = trans.basis.get_scale();
		}

	} // if not lerp
}

void Smooth::FrameUpdate()
{
	Spatial * pTarget = GetTarget();
	if (!pTarget)
	return;

	if (TestFlags(SF_DIRTY))
		RefreshTransform(pTarget);

	// interpolation fraction
	float f = Engine::get_singleton()->get_physics_interpolation_fraction();

	Vector3 ptNew = m_Prev.m_Transform.origin + (m_ptTranslateDiff * f);


//	Variant vf = f;
	//print_line("fraction " + itos(f * 1000.0f) + "\tsetting translation " + String(ptNew));

	// global or local?
	bool bGlobal = TestFlags(SF_GLOBAL_OUT);

	// simplified, only using translate
	// NOTE THIS IMPLIES LOCAL as global flag not set...
	if (m_Flags == (SF_ENABLED | SF_TRANSLATE))
	{
		set_translation(ptNew);
		return;
	}

	// send as a transform, the whole kabunga
	Transform trans;
	trans.origin = ptNew;

	// lerping
	if (TestFlags(SF_LERP))
	{
		//trans.basis = m_Prev.m_Basis.slerp(m_Curr.m_Basis, f);
		LerpBasis(m_Prev.m_Transform.basis, m_Curr.m_Transform.basis, trans.basis, f);
	}
	else
	{
		// slerping
		Quat qtRot = m_Prev.m_qtRotate.slerp(m_Curr.m_qtRotate, f);
		trans.basis.set_quat(qtRot);

		if (TestFlags(SF_SCALE))
		{
			Vector3 ptScale = ((m_Curr.m_ptScale - m_Prev.m_ptScale) * f) + m_Prev.m_ptScale;
			trans.basis.scale(ptScale);
		}
	}

	if (bGlobal)
		set_global_transform(trans);
	else
		set_transform(trans);
//	print_line("\tframe " + itos(f * 1000.0f) + " y is " + itos(y * 1000.0f) + " actual y " + itos(actual_y * 1000.0f));

}




void Smooth::LerpBasis(const Basis &from, const Basis &to, Basis &res, float f) const
{
	res = from;

	for (int n=0; n<3; n++)
		res.elements[n] = from.elements[n].linear_interpolate(to.elements[n], f);
}


