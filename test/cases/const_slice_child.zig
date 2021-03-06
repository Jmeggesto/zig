const assert = @import("std").debug.assert;

var argv: &&const u8 = undefined;

fn constSliceChild() {
    @setFnTest(this);

    const strs = ([]&const u8) {
        c"one",
        c"two",
        c"three",
    };
    argv = &strs[0];
    bar(strs.len);
}

fn foo(args: [][]const u8) {
    assert(args.len == 3);
    assert(streql(args[0], "one"));
    assert(streql(args[1], "two"));
    assert(streql(args[2], "three"));
}

fn bar(argc: usize) {
    const args = @alloca([]u8, argc);
    for (args) |_, i| {
        const ptr = argv[i];
        args[i] = ptr[0...strlen(ptr)];
    }
    foo(args);
}

fn strlen(ptr: &const u8) -> usize {
    var count: usize = 0;
    while (ptr[count] != 0; count += 1) {}
    return count;
}

fn streql(a: []const u8, b: []const u8) -> bool {
    if (a.len != b.len) return false;
    for (a) |item, index| {
        if (b[index] != item) return false;
    }
    return true;
}
