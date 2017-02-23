typedef struct {
	float2 ocl_xy;
	float ocl_z;
} HullEntry;

typedef struct {
	uchar4 v;
} IntermediateStorage;

<?php
require_once("linesweep.php");
?>

// from the LSAO paper, Equation (2)
// WARNING: the parameter must be d^2, not d.
__inline float falloff(float d_sqr) {
	return <?=A("falloff_radius")?>/(d_sqr+<?=A("falloff_radius")?>);
}

__inline float len_sqr_2(float2 v) {
	return v.x*v.x + v.y*v.y;
}

void norm_float2_inplace(float2 *v) {
	float l = sqrt(len_sqr_2(*v));
	(*v).x = (*v).x/l;
	(*v).y = (*v).y/l;
}

// from the LSAO paper, Equation (1) for sin(h)
float horizon_angle_sin(float2 p_xy, float p_z, float2 ocl_xy, float ocl_z, float2 step_direction) {
	float2 uk;
	uk.x = dot(p_xy, step_direction);
	uk.y = -p_z * 512;
	norm_float2_inplace(&uk);
	float2 o_xy = ocl_xy - p_xy;
	float2 hk;
	hk.x = dot(o_xy, step_direction);
	hk.y = ocl_z - p_z; // == o_z
	norm_float2_inplace(&hk);
	float sin_h = dot(hk, uk);
	return sin_h;
}

__inline float dst2_sqr(float2 a, float2 b) {
	float dx = a.x-b.x;
	float dy = a.y-b.y;
	return dx*dx+dy*dy;
}

// from the LSAO paper, the equation right before Equation (1)
// single sum term ("one K")
// with sin(t) := -0.5
__inline float occlusion(float2 p_xy, float p_z, float2 ocl_xy, float ocl_z, float2 step_direction) {
	float d_sqr = dst2_sqr(p_xy,ocl_xy);
	return -0.5 + (horizon_angle_sin(p_xy, p_z, ocl_xy, ocl_z, step_direction)+0.5) * falloff(d_sqr);
}

__kernel void lsdo(__global RayDescriptor* rays, __global DirectionDescriptor* directions, __global uint *ray_lookup_table, __global IntermediateStorage *intermediate_storage, __global float* input_image, __global uchar4 *horizon, uint2 ray_interval) {
	// init framework
	FrameworkState fs;
	init_framework(&fs, rays, directions, ray_interval);
	// local variables
	HullEntry hull[<?=A("hull_buffer_size")?>];
	int hull_size = 0;
	const float2 step_direction = normalize(fs.ray_step);
	// atan2pi outputs something in [-1,1]
	const uint angular_lookup_idx = (uint)round((atan2pi(step_direction.y,step_direction.x)+1) * <?=(A("horizon_angles_around")-1)/2.0?>);
	do {
		int2 coords = convert_int2(fs.current_position);
		coords = coords + (int2)(<?=round(A("image_width")/2)?>, <?=round(A("image_height")/2)?>);
		if (coords.x < 0 || coords.y < 0 || coords.x >= <?=A("image_width")?> || coords.y >= <?=A("image_height")?>) {
			intermediate_storage[fs.memory_idx].v = 0; // no occlusion from stuff outside the image
		} else {
			int idx = coords.y * <?=A("image_width")?> + coords.x;
			float p_z = input_image[idx];
			// pop the hull according to LSAO section 4.1
			while(hull_size > 1) {
				// PERF could cache the last occlusion result in case we need to check multiple points for pop
				if (occlusion(fs.current_position, p_z, hull[hull_size-1].ocl_xy,hull[hull_size-1].ocl_z,step_direction)
				  < occlusion(fs.current_position, p_z, hull[hull_size-2].ocl_xy,hull[hull_size-2].ocl_z,step_direction)) {
					// need to pop
					hull_size = hull_size - 1; // pop
				} else {
					// done popping
					break;
				}
			}
			// push
			if (hull_size >= <?=A("hull_buffer_size")?>) {
				// The hull buffer is full. This is bad.
				// Let's hope, the occluder behind the largest is not so important
				hull[<?=A("hull_buffer_size")-2?>] = hull[<?=A("hull_buffer_size")-1?>];
				hull_size = <?=A("hull_buffer_size")-1?>;
			}
			// actually push now
			hull[hull_size].ocl_xy = fs.current_position;
			hull[hull_size].ocl_z = p_z;
			hull_size = hull_size + 1;
			float2 ocl_xy = hull[hull_size-2].ocl_xy;
			float ocl_z  = hull[hull_size-2].ocl_z;
			// occlusion from largest occluder before self (at hull_size-2, because at hull_size-1 we would be occluding ourself)
			float result = occlusion(fs.current_position, p_z, ocl_xy, ocl_z, step_direction) / 2 + 0.5;
			// more occlusion should be darker, therefore inverted
			result = 1-result;
			if (result > 1) {
				result = 1;
			} else if (result < 0) {
				result = 0;
			}
			float d_h = ocl_z - p_z;
			float see_horizon = horizon_angle_sin(fs.current_position, p_z, ocl_xy, ocl_z, step_direction);
			if (see_horizon > 1) { see_horizon = 1; } // PERF maybe this is not needed
			if (see_horizon < 0) { see_horizon = 0; }
			uint horizon_sample_y = round((1-see_horizon) * <?=A("horizon_angles_up")-1?>);
			uchar4 horizon_color = horizon[horizon_sample_y * <?=A("horizon_angles_around")?> + angular_lookup_idx];
			uchar4 color_result = convert_uchar4(convert_float4(horizon_color) * (float4)(result));
			intermediate_storage[fs.memory_idx].v = color_result;
		}
	} while(sweep_step(&fs));
}

__kernel void gather_results(__global RayDescriptor* rays, __global DirectionDescriptor* directions, __global uint *ray_lookup_table, __global IntermediateStorage* sweep_data, __global uchar4* outimg, uint2 ray_interval) {
	uint2 pos = (uint2)(get_global_id(0),get_global_id(1));
	uint start_direction = rays[ray_interval.s0].direction_idx;
	uint end_direction = rays[ray_interval.s1-1].direction_idx+1;
	float2 posvec;
	posvec.x = (float)(pos.x) - <?=A("image_width")/2?>;
	posvec.y = (float)(pos.y) - <?=A("image_height")/2?>;
	int4 accumulator = convert_int4(outimg[pos.y * <?=A("image_width")?> + pos.x]);
	accumulator.x *= accumulator.w;
	accumulator.y *= accumulator.w;
	accumulator.z *= accumulator.w;
	int K = accumulator.w; // this counts how many directions actually had data
	for (uint i=start_direction; i < end_direction; i++) {
		__global IntermediateStorage* is_ptr = get_intermediate_storage(sweep_data, rays, directions, ray_lookup_table, posvec, i, ray_interval);
		if (!is_ptr || is_ptr->v.w == 0) {
			continue;
		}
		IntermediateStorage is = *is_ptr;
		accumulator += convert_int4(is.v);
		K++;
	}
	uchar4 result;
	if (K > 0) {
		result = convert_uchar4(accumulator/(int4)K);
	} else {
		result = 0;
	}
	if (end_direction == <?=A("sweep_directions")?>) {
		// last iteration
		// => w is alpha, set to opaque if any data is there
		if (K > 0) {
			result.w = 255;
		} else {
			// no data at all => invisible black
			result = 0;
		}
	} else {
		// store rolling average counter in the outimg.w
		result.w = K;
	}
	outimg[pos.y * <?=A("image_width")?> + pos.x] = result;
}

<?php
footer();
?>
