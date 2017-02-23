<?php
require_once("linesweep.php");
?>

#define EPSILON (1.0/(1<<16))

// the step vector mustn't be normalised!
int get_steps_to_triangle(__global double* triangles, double* current_position, double* step_vector) {
	// we could use a sophisticated bounding volume hierarchy here
	// move the ray description to double3 variables
	double3 orig, dir; // point and direction of the ray
	<?=assign("double[3]","current_position","double3","&orig")?>;
	<?=assign("double[3]","step_vector","double3","&dir")?>;
	// memorise best intersection "time" (i.e. intersection = p+t*d)
	double best_t = INFINITY;
	// iterate over the triangles, three vertices at a time
	for (uint t = 0; t < <?=A("num_triangles")*3?>; t+=3) {
		// intersection logic taken from https://www.lighthouse3d.com/tutorials/maths/ray-triangle-intersection/
		// load the current triangle
		double3 vert0 = vload3(  t, triangles);
		double3 vert1 = vload3(t+1, triangles);
		double3 vert2 = vload3(t+2, triangles);
		double3 edge1 = vert1-vert0;
		double3 edge2 = vert2-vert0;
		double3 pvec = cross(dir,edge2);
		double det = dot(edge1,pvec);
		if (det > -EPSILON && det < EPSILON) {
			continue;
		}
		double inv_det = 1/det;
		double3 tvec = orig-vert0;
		double u = dot(tvec,pvec)*inv_det;
		if (u < 0 || u > 1) {
			continue;
		}
		double3 qvec = cross(tvec,edge1);
		double v = dot(dir,qvec) * inv_det;
		if (v < 0 || u+v > 1) {
			continue;
		}
		double t = dot(edge2,qvec) * inv_det;
		if (t > EPSILON && t < best_t) {
			best_t = t;
		}
	}
	if (best_t < INFINITY) {
		// we have a hit
		return ceil(best_t);
	} else {
		// no hit
		return -1;
	}
}

__kernel void inside(__global RayDescriptor* rays, __global DirectionDescriptor* directions, uint2 ray_interval, __global double* triangles, __global int* output) {
	// init framework
	FrameworkState fs;
	init_framework(&fs, rays, directions, ray_interval);
	/*output[get_global_id(0)*4+1] = round(fs.ray_step[1] * 1000);
	output[get_global_id(0)*4+2] = round(fs.ray_step[2] * 1000);
	output[get_global_id(0)*4+3] = -1;
	return;*/
	// local variables
	bool inside = true;
	int steps_to_triangle = 1;
	// simulating a close triangle to trigger recalculation in the first step; this will then set inside to true.
	bool are_there_triangles = true;
	do {
		if (are_there_triangles) {
			steps_to_triangle--;
			if (steps_to_triangle <= 0) {
				steps_to_triangle = get_steps_to_triangle(triangles, fs.current_position, fs.ray_step);
				inside = !inside;
			}
			if (steps_to_triangle < 0) {
				are_there_triangles = false;
			}
		}
		double3 coords_tmp;
		<?=add("double[3]","fs.current_position","double",1,"double3","coords_tmp")?>; // make positive
		int3 coords;
		<?=mul("double3","coords_tmp","double",(A("outcubesize")-1)/2.0,"int3","coords")?>; // this will automatically be rounded correctly
		// in-bounds check; may be ommitable
		<?php for ($i=0; $i<3; $i++) {?>
		if (coords.s<?=$i?> < 0 || coords.s<?=$i?> >= <?=A("outcubesize")?>) {
			continue;
		}
		<?php } ?>
		int mempos = coords.s2 * <?=A("outcubesize")*A("outcubesize")?> + coords.s1 * <?=A("outcubesize")?> + coords.s0;
		atomic_add(output + mempos, (inside ? (1<<16)|1 : (1<<16)));
		/*if (fs.steps_left <= 5) {
			output[mempos] = (1<<16)|1;
		} else {
			output[mempos] = inside ? (1<<16)|1 : (1<<16);
		}*/
	} while(sweep_step(&fs));
}

<?php
footer();
?>
