
int main() {

	//===-------------------------------------------------------------------------------------------------------------------------------- NULL TO POINTER ---===

	#pragma test expect_ir("ptr_null(type(int<4>), type(f), type(f))")
	(int*)0;

	#pragma test expect_ir("ptr_null(type(real<4>), type(t), type(f))")
	(const float*)0;

	#pragma test expect_ir("ptr_null(type(real<4>), type(f), type(f))")
	(float* const)0;

	#pragma test expect_ir("ptr_null(type(int<4>), type(t), type(f))")
	(const int* const)0;

	//===---------------------------------------------------------------------------------------------------------------------------------- NUMERIC CASTS ---===

	// IntegralToFloating
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; decl ref<real<4>,f,f> v1 = var(num_cast(*v0,type(real<4>))); }")
	{
		int x;
		float y = x;
	}
	
	// FloatingToIntegral
	#pragma test expect_ir("{ decl ref<real<4>,f,f> v0; decl ref<int<4>,f,f> v1 = var(num_cast(*v0,type(int<4>))); }")
	{
		float x;
		int y = x;
	}
	
	// FloatingCast
	#pragma test expect_ir("{ decl ref<real<4>,f,f> v0; decl ref<real<8>,f,f> v1 = var(num_cast(*v0,type(real<8>))); }")
	{
		float x;
		double y = x;
	}

	// IntegralCast
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; decl ref<uint<4>,f,f> v1 = var(num_cast(*v0,type(uint<4>))); }")
	{
		int x;
		unsigned y = x;
	}
	#pragma test expect_ir("{ decl ref<uint<8>,f,f> v0; decl ref<int<4>,f,f> v1 = var(num_cast(*v0,type(int<4>))); }")
	{
		unsigned long x;
		int y = x;
	}
	#pragma test expect_ir("{ decl ref<char,f,f> v0; decl ref<int<1>,f,f> v1 = var(num_cast(*v0,type(int<1>))); }")
	{
		char x;
		signed char y = x;
	}
	
	//===---------------------------------------------------------------------------------------------------------------------------------- POINTER CASTS ---===
	
	// implicit type change
	#pragma test expect_ir("{ decl ref<ptr<unit,f,f>,f,f> v0; decl ref<ptr<int<4>,f,f>,f,f> v1 = var(ptr_reinterpret(*v0, type(int<4>))); }")
	{
		void* x;
		int* y = x;
	}
	
	// explicit type change
	#pragma test expect_ir("{ decl ref<ptr<unit,f,f>,f,f> v0; decl ref<ptr<int<4>,f,f>,f,f> v1 = var(ptr_reinterpret(*v0, type(int<4>))); }")
	{
		void* x;
		int* y = (int*)x;
	}

	// implicit qualifier change
	#pragma test expect_ir("{ decl ref<ptr<unit,t,f>,f,f> v0; decl ref<ptr<unit,f,f>,f,f> v1 = var(ptr_cast(*v0, type(f), type(f))); }")
	{
		const void* x;
		void* y = x;
	}
	
	// explicit qualifier change
	#pragma test expect_ir("{ decl ref<ptr<unit,t,f>,f,f> v0; decl ref<ptr<unit,f,f>,f,f> v1 = var(ptr_cast(*v0, type(f), type(f))); }")
	{
		const void* x;
		void* y = (void*)x;
	}
	
	// implicit type + qualifier change
	#pragma test expect_ir("{ decl ref<ptr<unit,t,f>,f,f> v0; decl ref<ptr<int<4>,f,f>,f,f> v1 = var(ptr_cast(ptr_reinterpret(*v0, type(int<4>)), type(f), type(f))); }")
	{
		const void* x;
		int* y = x;
	}
	
	// explicit type + qualifier change
	#pragma test expect_ir("{ decl ref<ptr<unit,t,f>,f,f> v0; decl ref<ptr<int<4>,f,f>,f,f> v1 = var(ptr_cast(ptr_reinterpret(*v0, type(int<4>)), type(f), type(f))); }")
	{
		const void* x;
		int* y = (int*)x;
	}

	//===------------------------------------------------------------------------------------------------------------------------------------- MISC CASTS ---===

	// NoOp - casting between identical types
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; *v0; }")
	{
		int x;
		(int)x;
	}

	//===----------------------------------------------------------------------------------------------------------------------------------- TO BOOL CASTS---===

	// char to bool (C99's native boolean type is _Bool)
	#pragma test expect_ir("{ decl ref<char,f,f> v0; (*v0!='\0'); }")
	{
		char x;
		(_Bool)x;
	}

	// integral to bool
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; (*v0!=0); }")
	{
		int x;
		(_Bool)x;
	}

	// integral to bool
	#pragma test expect_ir("{ decl ref<uint<8>,f,f> v0; (*v0!=0ul); }")
	{
		unsigned long x;
		(_Bool)x;
	}

	// float to bool
	#pragma test expect_ir("{ decl ref<real<4>,f,f> v0; (*v0!=0.0f); }")
	{
		float x;
		(_Bool)x;
	}

	// pointer to bool
	#pragma test expect_ir("{ decl ref<ptr<real<4>,f,f>,f,f> v0; ptr_ne(*v0, ptr_null(type(real<4>), type(f), type(f))); }")
	{
		float* x;
		(_Bool)x;
	}

}
