pragma abicoder               v2;

contract C {
  uint[][] tmp_i;
  function i(uint[][] calldata s) external { tmp_i = s; }
}

// ----
