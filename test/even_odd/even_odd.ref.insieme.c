// --- Generated Inspire Code ---
#include <stddef.h>
#define bool int
#define true 1
#define false 0
// --- Entry Point ---

// start code fragment :: Prototype for external function: printf //
int printf(char*, ...);

// start code fragment :: Prototype of even //
int even(unsigned int);

// start code fragment :: Prototype of odd //
int odd(unsigned int);

// start code fragment :: Definition of __insieme_supp_0 //
int __insieme_supp_0() {
	return 1;
}


// start code fragment :: Definitions for function type: __insieme_funType_type_1 //
// Abstract prototype for lambdas of type __insieme_funType_type_1
struct __insieme_funType_type_1 { 
    int(*fun)(void*);
    const size_t size;
};

// Type safe function for invoking lambdas of type __insieme_funType_type_1
int call___insieme_funType_type_1(struct __insieme_funType_type_1* lambda) { return lambda->fun(lambda); }

// start code fragment :: Definitions for function type: __insieme_funType_type_3 //
// Abstract prototype for lambdas of type __insieme_funType_type_3
struct __insieme_funType_type_3 { 
    int(*fun)(void*,unsigned int);
    const size_t size;
};

// Type safe function for invoking lambdas of type __insieme_funType_type_3
int call___insieme_funType_type_3(struct __insieme_funType_type_3* lambda,unsigned int p1) { return lambda->fun(lambda,p1); }

// start code fragment :: Definitions for function type: __insieme_funType_type_2 //
// Abstract prototype for lambdas of type __insieme_funType_type_2
struct __insieme_funType_type_2 { 
    int(*fun)(void*);
    const size_t size;
    struct __insieme_funType_type_3* p0;
    unsigned int p1;
};

// start code fragment :: Definition of __insieme_supp_4 //
int __insieme_supp_4(void* _capture) {
	// --------- Captured Stuff - Begin -------------
	struct __insieme_funType_type_3* var_7 = ((struct __insieme_funType_type_2*)_capture)->p0;
	unsigned int var_8 = ((struct __insieme_funType_type_2*)_capture)->p1;
	// --------- Captured Stuff -  End  -------------
	return var_7->fun(var_7, (var_8-((unsigned int)(1))));
}


// start code fragment :: Definition of even //
int even(unsigned int x) {
	{
		return ((((x==((unsigned int)(0))))?(__insieme_supp_0):(((struct __insieme_funType_type_1*)(&((struct __insieme_funType_type_2){&__insieme_supp_4, 0, odd, x}))))));;
	}
}


// start code fragment :: Definition of __insieme_supp_5 //
int __insieme_supp_5() {
	return 0;
}


// start code fragment :: Definition of __insieme_supp_6 //
int __insieme_supp_6(void* _capture) {
	// --------- Captured Stuff - Begin -------------
	struct __insieme_funType_type_3* var_12 = ((struct __insieme_funType_type_2*)_capture)->p0;
	unsigned int var_13 = ((struct __insieme_funType_type_2*)_capture)->p1;
	// --------- Captured Stuff -  End  -------------
	return var_12->fun(var_12, (var_13-((unsigned int)(1))));
}


// start code fragment :: Definition of odd //
int odd(unsigned int x) {
	{
		return ((((x==((unsigned int)(0))))?(__insieme_supp_5):(((struct __insieme_funType_type_1*)(&((struct __insieme_funType_type_2){&__insieme_supp_6, 0, even, x}))))));;
	}
}


// start code fragment :: Definition of __insieme_supp_7 //
char* __insieme_supp_7() {
	return ((char*)("true"));
}


// start code fragment :: Definition of __insieme_supp_8 //
char* __insieme_supp_8() {
	return ((char*)("false"));
}


// start code fragment :: Definition of main //
int main(int argc, char** argv) {
	{
		int x = 10;
		printf(((char*)("x=%d\n")), x);
		printf(((char*)("even(x)=%s\n")), (((((bool)(even(((unsigned int)(x))))))?(__insieme_supp_7):(__insieme_supp_8))));
		printf(((char*)("odd(x)=%s\n")), (((((bool)(odd(((unsigned int)(x))))))?(__insieme_supp_7):(__insieme_supp_8))));
		return 0;;
	}
}

