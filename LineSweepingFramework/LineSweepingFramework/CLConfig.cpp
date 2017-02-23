#include "CLConfig.hpp"

CLConfig* CLConfig::default_config() {
	CLConfig* ret = new CLConfig();
	ret->context = createCLContext(CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_GPU);
	ret->devices = ret->context.getInfo<CL_CONTEXT_DEVICES>();
	if (ret->devices.size() > 1) {
		std::cerr << "Warning: There are " << ret->devices.size() << " OpenCL devices, but the CLConfig::default_config will only use the \"first\" one." << std::endl;
	}
	ret->queue = cl::CommandQueue(ret->context, ret->devices[0]);
	return ret;
}
