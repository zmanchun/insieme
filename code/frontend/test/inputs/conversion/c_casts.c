
int main() {

	//===-------------------------------------------------------------------------------------------------------------------------------- NULL TO POINTER ---===

	#pragma test expect_ir("type_cast(0, type(ptr<int<4>,f,f>))")
	(int*)0;

	#pragma test expect_ir("type_cast(0, type(ptr<real<4>,t,f>))")
	(const float*)0;

	#pragma test expect_ir("type_cast(0, type(ptr<real<4>,f,f>))")
	(float* const)0;

	#pragma test expect_ir("type_cast(0, type(ptr<int<4>,t,f>))")
	(const int* const)0;

	//===-------------------------------------------------------------------------------------------------------------------------------- POINTER TO BOOL ---===

	// TODO: move this somewhere else? Because there is no actual clang::CK_PointerToBoolean involved here
	#pragma test expect_ir("{ decl ref<ptr<int<4>,f,f>,f,f> v0; if(ptr_eq((*v0), ptr_null)) { } }")
	{
		int* p;
		if(p) { }
	}
	
	//===---------------------------------------------------------------------------------------------------------------------------------- NUMERIC CASTS ---===

	// IntegralToFloating
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; decl ref<real<4>,f,f> v1 = var(type_cast(*v0,type(real<4>))); }")
	{
		int x;
		float y = x;
	}
	
	// FloatingToIntegral
	#pragma test expect_ir("{ decl ref<real<4>,f,f> v0; decl ref<int<4>,f,f> v1 = var(type_cast(*v0,type(int<4>))); }")
	{
		float x;
		int y = x;
	}
	
	// FloatingCast
	#pragma test expect_ir("{ decl ref<real<4>,f,f> v0; decl ref<real<8>,f,f> v1 = var(type_cast(*v0,type(real<8>))); }")
	{
		float x;
		double y = x;
	}

	// IntegralCast
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; decl ref<uint<4>,f,f> v1 = var(type_cast(*v0,type(uint<4>))); }")
	{
		int x;
		unsigned y = x;
	}
	#pragma test expect_ir("{ decl ref<uint<8>,f,f> v0; decl ref<int<4>,f,f> v1 = var(type_cast(*v0,type(int<4>))); }")
	{
		unsigned long x;
		int y = x;
	}
	#pragma test expect_ir("{ decl ref<char,f,f> v0; decl ref<int<1>,f,f> v1 = var(type_cast(*v0,type(int<1>))); }")
	{
		char x;
		signed char y = x;
	}	

}
