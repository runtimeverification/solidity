contract test { fallback() external { uint x = 3; int y = -4; x ** y; } }
// ----
// TypeError 2271: (62-68): Operator ** not compatible with types uint and int. Exponentiation power is not allowed to be a signed integer type.
