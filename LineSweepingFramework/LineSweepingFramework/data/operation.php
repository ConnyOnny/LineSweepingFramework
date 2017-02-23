<?php

require_once("simpletype.php");

function op() {
	$args = func_get_args();
	return OperationManager::need_function($args);
}
function mul() {
	return OperationManager::need_function(array_merge(["mul"],func_get_args()));
}
function div() {
	return OperationManager::need_function(array_merge(["div"],func_get_args()));
}
function add() {
	return OperationManager::need_function(array_merge(["add"],func_get_args()));
}
function sub() {
	return OperationManager::need_function(array_merge(["sub"],func_get_args()));
}
function conv() {
	return OperationManager::need_function(array_merge(["conv"],func_get_args()));
}
function dot() {
	return OperationManager::need_function(array_merge(["dot"],func_get_args()));
}
function norm() {
	return OperationManager::need_function(array_merge(["norm"],func_get_args()));
}
function length() {
	return OperationManager::need_function(array_merge(["length"],func_get_args()));
}
function assign() {
	return OperationManager::need_function(array_merge(["assign"],func_get_args()));
}

function access_field($t,$i) {
	$a = arity($t);
	if ($a == 1) {
		return "";
	} else {
		return ".s".$i;
	}
}

class OperationException extends Exception {}

class OperationManager {
	private static $needed_functions = array();
	private static $op_to_class = array(
		"add" => "Addition",
		"sub" => "Subtraction",
		"mul" => "Multiplication",
		"div" => "Division",
		"dot" => "DotProduct",
		"conv" => "Conversion",
		"norm" => "Normalise",
		"length" => "Length",
		"assign" => "Assign",
	);
	private function __construct() {}
	private static function funstr($op, $t1, $p1, $t2, $p2, $tr, $r) {
		if (!preg_match('/^[a-zA-Z0-9\[\]]*$/', implode("",[$op,$t1,$t2,$tr]))) {
			throw new OperationException("Use of invalid characters in type name.");
		}
		$s = implode('_', [$op,$t1,$t2,$tr]);
		if (isset($r)) {
			$s .= "_inplace";
		}
		return $s;
	}
	public static function need_function($args) {
		$op = $args[0];
		$op_instance = new self::$op_to_class[$op]($args);
		array_push(self::$needed_functions, $op_instance);
		return $op_instance->callstr($args);
	}
	public static function declarations() {
		$ret = "";
		$built = [];
		foreach (self::$needed_functions as $op_instance) {
			$fname = $op_instance->funstr();
			// skip if we already built this exact function
			if (!in_array($fname, $built)) {
				$ret .= $op_instance->build();
				array_push($built, $fname);
			}
		}
		return $ret;
	}
}

abstract class Operation {}

/*
class Conversion extends Operation {
	// this is done inline, so build and funstr return nothing and callstr does all the work
	public function __construct($args) {}
	public function callstr($args) {
		assert(count($args)==4);
		assert($args[0] == "conv");
		$t_in = $args[1]);
		$t_out = $args[2];
		$v = $args[3];
		$ar_in = arity($t_in);
		$ar_out = arity($t_out);
		assert($ar_in == $ar_out, "Cannot convert between vectors of different sizes");
		$ar = $ar_in;
		$single_cast_string = "(".scalar_type($t_out).")";
		$mustround = is_int_type($t_out) && !(is_int_type($t_in));
		$roundstr = $mustround ? "round" : "";
		if ($ar == 1) {
			// people should probably cast directly in this case, as it will be less cumbersome
			return "($single_cast_string$roundstr($v))";
		}
		$substrings = array();
		for ($i=0; $i<$ar; $i++) {
			array_push($substrings, $single_cast_string . $roundstr . "($v".access_field($t_in,$i) . ")"); 
		}
		$convstring = join(", ",$substrings);
		return "($t_out)($convstring)";
	}
	public function funstr() {
		return "";
	}
	public function build() {
		return "";
	}
}
*/

class Assign extends Operation {
	public function __construct($args) {
		assert($args[0] == "assign");
		assert(count($args) == 5);
		$this->t_in = SimpleType::from_string($args[1]);
		$this->t_out = SimpleType::from_string($args[3]);
		$s_parts = [$args[0], $this->t_in->escaped(), $this->t_out->escaped()];
		$this->s = implode('_', $s_parts);
	}
	public function callstr($args) {
		return $this->s."(".$args[2].",".$args[4].")";
	}
	public function funstr() {
		return $this->s;
	}
	public function build() {
		$mustround = $this->t_out->is_int_type() && !$this->t_in->is_int_type();
		$roundstr = $mustround ? "round" : "";
		$maxarity = max($this->t_in->arity(), $this->t_out->arity());
		$pbrs = $this->t_out->pass_by_reference()?"":"*"; // pass by reference star
		$header = "void {$this->s} ({$this->t_in->paramdecl()} in, {$this->t_out->paramdecl()}$pbrs out) {" . PHP_EOL;
		$body = "";
		for ($i=0; $i<$maxarity; $i++) {
			$body .= "\t($pbrs out){$this->t_out->access($i)} = $roundstr(in{$this->t_in->access($i)});" . PHP_EOL;
		}
		return $header.$body.'}'.PHP_EOL;
	}
}

class Normalise extends Operation {
	public function __construct($args) {
		assert($args[0] == "norm");
		assert(count($args) == 3);
		$parts = [$args[0],$args[1]];
		$this->s = implode('_', $parts);
		$this->t = $parts[1];
		assert(!is_int_type($this->t), "Cannot normalise integer vectors.");
	}
	public function callstr($args) {
		return $this->s."(".$args[2].")";
	}
	public function funstr() {
		return $this->s;
	}
	public function build() {
		$arity = arity($this->t);
		assert($arity > 1, "Normalising works only on vectors; not on scalars.");
		$lenarr = [];
		$divarr = [];
		for ($i=0; $i < $arity; $i++) {
			$x = "v".access_field($this->t,$i);
			array_push($lenarr, "$x*$x");
			array_push($divarr, "$x/len");
		}
		$lenstr = implode("+",$lenarr);
		$divstr = implode(",",$divarr);
		return
"{$this->t} {$this->s} ({$this->t} v) {" . PHP_EOL .
"	".scalar_type($this->t)." len = sqrt($lenstr);" . PHP_EOL .
"	return ({$this->t})($divstr);". PHP_EOL .
"}" . PHP_EOL;
	}
}

class Length extends Operation {
	public function __construct($args) {
		assert($args[0] == "length");
		assert(count($args) == 3);
		$parts = [$args[0],$args[1]];
		$this->s = implode('_', $parts);
		$this->t = $parts[1];
		assert(!is_int_type($this->t));
	}
	public function callstr($args) {
		return $this->s."(".$args[2].")";
	}
	public function funstr() {
		return $this->s;
	}
	public function build() {
		$arity = arity($this->t);
		assert($arity > 1, "Length calculation works only on vectors; not on scalars.");
		$lenarr = [];
		for ($i=0; $i < $arity; $i++) {
			$x = "v".access_field($this->t,$i);
			array_push($lenarr, "$x*$x");
		}
		$lenstr = implode("+",$lenarr);
		return
scalar_type($this->t)." {$this->s} ({$this->t} v) {" . PHP_EOL .
"	return sqrt($lenstr);" . PHP_EOL .
"}" . PHP_EOL;
	}
}

abstract class BinaryOperation extends Operation {
	public function __construct($args) {
		assert(sizeof($args) == 7, "unexpected number of arguments for a binary operation");
		$this->t1 = SimpleType::from_string($args[1]);
		$this->t2 = SimpleType::from_string($args[3]);
		$this->tr = SimpleType::from_string($args[5]);
		$parts = [$args[0], $this->t1->escaped(), $this->t2->escaped(), $this->tr->escaped()];
		$this->s = implode('_', $parts);
		$this->op = $parts[0];
	}
	public function callstr($args) {
		$pbrs = $this->tr->pass_by_reference()?"":"&";
		return $this->s."(".$args[2].",".$args[4].",$pbrs(".$args[6]."))";
	}
	public function funstr() {
		return $this->s;
	}
	public function build() {
		$maxarity = max($this->t1->arity(),$this->t2->arity());
		$mustround = $this->tr->is_int_type() && !($this->t1->is_int_type() && ($this->t2->is_int_type()));
		$roundstr = $mustround ? "round" : "";
		$single_cast_string = "(".$this->tr->get_scalar().")";
		$calcstrings = array();
		$pbrs = $this->tr->pass_by_reference()?"":"*"; // pass by reference star
		if ($maxarity <= 1) {
			throw new Exception("Not implemented for scalars. Please do this manually.");
		}
		for ($i=0; $i<$maxarity; $i++) {
			array_push($calcstrings, $single_cast_string . $roundstr . "(p1".$this->t1->access($i) . $this->scalar_operator . "p2".$this->t2->access($i) . ")");
		}
		$header = "void {$this->s}({$this->t1->paramdecl()} p1, {$this->t2->paramdecl()} p2, {$this->tr->paramdecl()}$pbrs r) {" . PHP_EOL;
		if ($this->tr->arity == 1) {
			$calcstring = "	$pbrs r = ({$this->tr})(".implode($this->join_operator, $calcstrings).");".PHP_EOL;
		} else {
			$calcstring = "";
			$i = 0;
			foreach($calcstrings as $s) {
				$calcstring .= "\t($pbrs r){$this->tr->access($i)} = $s;".PHP_EOL;
				$i++;
			}
		}
		return $header.$calcstring."}".PHP_EOL;
	}
}

abstract class ScalarBinaryOperation extends BinaryOperation {
	protected $join_operator = ",";
}

class Addition extends ScalarBinaryOperation {
	protected $scalar_operator = "+";
}

class Subtraction extends ScalarBinaryOperation {
	protected $scalar_operator = "-";
}

class Multiplication extends ScalarBinaryOperation {
	protected $scalar_operator = "*";
}

class Division extends ScalarBinaryOperation {
	protected $scalar_operator = "/";
}

class DotProduct extends BinaryOperation {
	protected $scalar_operator = "*";
	protected $join_operator = "+";
}

