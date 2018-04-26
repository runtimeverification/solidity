contract test {
    struct Data { uint a; bool b; }

	mapping(uint => Data[]) public to_multiple_map;
}
