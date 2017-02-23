<?php
die("This code needs an update to the latest API version.");
require_once("functions.php");

?>

// input: 32.0 format, output: treat as 16.16 format
__inline uint quadraticAttenuation (uint dst) {
	dst += 15; // value chosen to resemble regular diffusion inpainting
	return (1<<16) / (dst*dst);
}

<?php
sweep_inner_add_local_variable("uchar4", "last_seen_color", "0");
sweep_inner_add_local_variable("int", "last_seen_distance", "-1");
?>

__inline void sweep_inner(float2* current_position, float2* step_vec, uchar4* last_seen_color, int* last_seen_distance, __global uchar4* input_image, __global uint* accu_buffer) {
	int2 coords = <?php conv("float2", "int2", "*current_position"); ?>;
	coords = coords + (int2)<?php echo"(", round(A("image_width")/2), ", ", round(A("image_height")/2), ");";?>
	if (coords.x < 0 || coords.y < 0 || coords.x >= <?=A("image_width")?> || coords.y >= <?=A("image_height")?>) {
		return;
	}
	int idx = coords.y * <?=A("image_width")?> + coords.x;
	uchar4 color_here = input_image[idx];
	if (color_here.w != 0) {
		// has information, doesn't need information
		*last_seen_color = color_here;
		*last_seen_distance = 0;
	}
	if (*last_seen_distance >= 0) { // have I ever seen anything?
		uint actualDistance = *last_seen_distance * <?=A("gridsize")?>;
		uint distanceMultiplier = quadraticAttenuation(actualDistance); // 16.16 format
		uint4 attenuatedValue = <?php mul("uint","uchar4","uint4","distanceMultiplier","*last_seen_color");?>;
		uint idx4 = 4*idx;
		atomic_add(accu_buffer+idx4, attenuatedValue.x);
		atomic_add(accu_buffer+idx4+1, attenuatedValue.y);
		atomic_add(accu_buffer+idx4+2, attenuatedValue.z);
		atomic_add(accu_buffer+idx4+3, distanceMultiplier); // 16.16
	}
}

__kernel void normaliser(__global uint* accu_buffer, __global uchar4* output_image) {
	uint idx = get_global_id(0);
	uint idx4 = 4*idx;
	uint4 value = (uint4)(accu_buffer[idx4],accu_buffer[idx4+1],accu_buffer[idx4+2],accu_buffer[idx4+3]);
	uint divisor = value.w;
	if (divisor == 0) {
		output_image[idx] = (uchar4)(0,255,0,0); // transparent green as error color
	} else {
		output_image[idx] = <?php div("uint4","uint","uchar4","value","divisor") ?>;
		output_image[idx].w = 255;
	}
}

<?php
footer();
?>
