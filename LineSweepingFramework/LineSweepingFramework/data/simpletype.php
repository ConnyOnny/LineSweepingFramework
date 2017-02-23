<?php

abstract class SimpleType {
	private static $subtypes = array(
		"ScalarType",
		"VectorType",
		"ArrayType",
	);
	public static function from_string($s) {
		if (substr($s,0,2) == "#G") {
			$_global = true;
			$s = substr($s,2);
		} else {
			$_global = false;
		}
		foreach (SimpleType::$subtypes as $t) {
			if (preg_match($t::regex, $s) === 1) {
				$ret = new $t($s);
			}
		}
		if (!isset($ret)) {
			throw new Exception("unknown typedef: $s");
		}
		$ret->_global = $_global;
		return $ret;
	}
	protected function gstr() {
		return ($this->_global?"global":"");
	}
	public function paramdecl() {
		return $this->_paramdecl();
	}
	public function get_scalar() {
		return $this->name;
	}
	public function is_int_type() {
		$INT_TYPES_SUBSTRINGS = ["char","short","int","long","size_t","ptr"];
		$t = $this->get_scalar();
		foreach ($INT_TYPES_SUBSTRINGS as $s) {
			if (strpos($t, $s) !== FALSE)
				return TRUE;
		}
		return FALSE;
	}
	public function to_string() {
		return $this->gstr() . $this->_to_string();
	}
	// override if there are characters to be escaped
	public function _escaped() {
		return $this->to_string();
	}
	public function escaped() {
		return $this->gstr() . $this->_escaped();
	}
	public function pass_by_reference() {
		return false;
	}
}

class ScalarType extends SimpleType {
	const regex = '/^[a-zA-Z_]+$/';
	public function __construct($s) {
		assert(preg_match(ScalarType::regex, $s) === 1);
		$this->name = $s;
	}
	public function decl0() {
		return $this->name;
	}
	public function decl1() {
		return "";
	}
	public function _paramdecl() {
		return $this->name;
	}
	public function access($i) {
		return "";
	}
	public function arity() {
		return 1;
	}
	public function _to_string() {
		return $this->name;
	}
}

class VectorType extends SimpleType {
	const regex = '/^([a-zA-Z_]+)([0-9]+)$/';
	public function __construct($s) {
		$a = array();
		if (preg_match(VectorType::regex, $s, $a) !== 1) {
			throw new Exception("Not a valid VectorType definition: '$s'");
		}
		assert(count($a)==3);
		assert($a[0] == $s);
		$this->name = $a[1];
		$this->arity = intval($a[2]);
	}
	public function decl0() {
		return $this->name.$this->arity;
	}
	public function decl1() {
		return "";
	}
	public function _paramdecl() {
		return $this->name.$this->arity;
	}
	public function access($i) {
		assert($i < $this->arity);
		return ".s$i";
	}
	public function arity() {
		return $this->arity;
	}
	public function _to_string() {
		return $this->name.$this->arity;
	}
}

class ArrayType extends SimpleType {
	const regex = '/^([a-zA-Z_]+)\[([0-9]+)\]$/';
	public function __construct($s) {
		$a = array();
		if (preg_match(ArrayType::regex, $s, $a) !== 1) {
			throw new Exception("Not a valid ArrayType definition: '$s'");
		}
		assert(count($a)==3);
		assert($a[0] == $s);
		$this->name = $a[1];
		$this->arity = intval($a[2]);
	}
	public function decl0() {
		return $this->name;
	}
	public function decl1() {
		return "[{$this->arity}]";
	}
	public function _paramdecl() {
		return ($this->_global?"__global ":"") . $this->name.'*';
	}
	public function access($i) {
		assert($i < $this->arity);
		return "[$i]";
	}
	public function arity() {
		return $this->arity;
	}
	public function _to_string() {
		return "{$this->name}[{$this->arity}]";
	}
	public function _escaped() {
		return "{$this->name}_{$this->arity}";
	}
	public function pass_by_reference() {
		return true;
	}
}
