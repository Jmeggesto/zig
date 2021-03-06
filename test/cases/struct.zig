const assert = @import("std").debug.assert;

const StructWithNoFields = struct {
    fn add(a: i32, b: i32) -> i32 { a + b }
};
const empty_global_instance = StructWithNoFields {};

fn callStructStaticMethod() {
    @setFnTest(this);
    const result = StructWithNoFields.add(3, 4);
    assert(result == 7);
}

fn returnEmptyStructInstance() -> StructWithNoFields {
    @setFnTest(this);
    return empty_global_instance;
}

const should_be_11 = StructWithNoFields.add(5, 6);

fn invokeStaticMethodInGlobalScope() {
    @setFnTest(this);
    assert(should_be_11 == 11);
}

fn voidStructFields() {
    @setFnTest(this);

    const foo = VoidStructFieldsFoo {
        .a = void{},
        .b = 1,
        .c = void{},
    };
    assert(foo.b == 1);
    assert(@sizeOf(VoidStructFieldsFoo) == 4);
}
const VoidStructFieldsFoo = struct {
    a : void,
    b : i32,
    c : void,
};


pub fn structs() {
    @setFnTest(this);

    var foo: StructFoo = undefined;
    @memset((&u8)(&foo), 0, @sizeOf(StructFoo));
    foo.a += 1;
    foo.b = foo.a == 1;
    testFoo(foo);
    testMutation(&foo);
    assert(foo.c == 100);
}
const StructFoo = struct {
    a : i32,
    b : bool,
    c : f32,
};
fn testFoo(foo : StructFoo) {
    assert(foo.b);
}
fn testMutation(foo : &StructFoo) {
    foo.c = 100;
}


const Node = struct {
    val: Val,
    next: &Node,
};

const Val = struct {
    x: i32,
};

fn structPointToSelf() {
    @setFnTest(this);

    var root : Node = undefined;
    root.val.x = 1;

    var node : Node = undefined;
    node.next = &root;
    node.val.x = 2;

    root.next = &node;

    assert(node.next.next.next.val.x == 1);
}

fn structByvalAssign() {
    @setFnTest(this);

    var foo1 : StructFoo = undefined;
    var foo2 : StructFoo = undefined;

    foo1.a = 1234;
    foo2.a = 0;
    assert(foo2.a == 0);
    foo2 = foo1;
    assert(foo2.a == 1234);
}

fn structInitializer() {
    const val = Val { .x = 42 };
    assert(val.x == 42);
}


fn fnCallOfStructField() {
    @setFnTest(this);

    assert(callStructField(Foo {.ptr = aFunc,}) == 13);
}

const Foo = struct {
    ptr: fn() -> i32,
};

fn aFunc() -> i32 { 13 }

fn callStructField(foo: Foo) -> i32 {
    return foo.ptr();
}


fn storeMemberFunctionInVariable() {
    @setFnTest(this);

    const instance = MemberFnTestFoo { .x = 1234, };
    const memberFn = MemberFnTestFoo.member;
    const result = memberFn(instance);
    assert(result == 1234);
}
const MemberFnTestFoo = struct {
    x: i32,
    fn member(foo: MemberFnTestFoo) -> i32 { foo.x }
};


fn callMemberFunctionDirectly() {
    @setFnTest(this);

    const instance = MemberFnTestFoo { .x = 1234, };
    const result = MemberFnTestFoo.member(instance);
    assert(result == 1234);
}

fn memberFunctions() {
    @setFnTest(this);

    const r = MemberFnRand {.seed = 1234};
    assert(r.getSeed() == 1234);
}
const MemberFnRand = struct {
    seed: u32,
    pub fn getSeed(r: &const MemberFnRand) -> u32 {
        r.seed
    }
};

fn returnStructByvalFromFunction() {
    @setFnTest(this);

    const bar = makeBar(1234, 5678);
    assert(bar.y == 5678);
}
const Bar = struct {
    x: i32,
    y: i32,
};
fn makeBar(x: i32, y: i32) -> Bar {
    Bar {
        .x = x,
        .y = y,
    }
}

fn emptyStructMethodCall() {
    @setFnTest(this);

    const es = EmptyStruct{};
    assert(es.method() == 1234);
}
const EmptyStruct = struct {
    fn method(es: &const EmptyStruct) -> i32 {
        1234
    }
};


fn returnEmptyStructFromFn() {
    @setFnTest(this);

    _ = testReturnEmptyStructFromFn();
}
const EmptyStruct2 = struct {};
fn testReturnEmptyStructFromFn() -> EmptyStruct2 {
    EmptyStruct2 {}
}

fn passSliceOfEmptyStructToFn() {
    @setFnTest(this);

    assert(testPassSliceOfEmptyStructToFn([]EmptyStruct2{ EmptyStruct2{} }) == 1);
}
fn testPassSliceOfEmptyStructToFn(slice: []const EmptyStruct2) -> usize {
    slice.len
}

const APackedStruct = packed struct {
    x: u8,
    y: u8,
};

fn packedStruct() {
    @setFnTest(this);

    var foo = APackedStruct {
        .x = 1,
        .y = 2,
    };
    foo.y += 1;
    const four = foo.x + foo.y;
    assert(four == 4);
}


const u2 = @intType(false, 2);
const u3 = @intType(false, 3);

const BitField1 = packed struct {
    a: u3,
    b: u3,
    c: u2,
};

const bit_field_1 = BitField1 {
    .a = 1,
    .b = 2,
    .c = 3,
};

fn bitFieldAccess() {
    @setFnTest(this);

    var data = bit_field_1;
    assert(getA(&data) == 1);
    assert(getB(&data) == 2);
    assert(getC(&data) == 3);
    comptime assert(@sizeOf(BitField1) == 1);

    data.b += 1;
    assert(data.b == 3);

    data.a += 1;
    assert(data.a == 2);
    assert(data.b == 3);
}

fn getA(data: &const BitField1) -> u3 {
    return data.a;
}

fn getB(data: &const BitField1) -> u3 {
    return data.b;
}

fn getC(data: &const BitField1) -> u2 {
    return data.c;
}

const u24 = @intType(false, 24);
const Foo24Bits = packed struct {
    field: u24,
};
const Foo96Bits = packed struct {
    a: u24,
    b: u24,
    c: u24,
    d: u24,
};

fn packedStruct24Bits() {
    @setFnTest(this);

    comptime {
        assert(@sizeOf(Foo24Bits) == 3);
        assert(@sizeOf(Foo96Bits) == 12);
    }

    var value = Foo96Bits {
        .a = 0,
        .b = 0,
        .c = 0,
        .d = 0,
    };
    value.a += 1;
    assert(value.a == 1);
    assert(value.b == 0);
    assert(value.c == 0);
    assert(value.d == 0);

    value.b += 1;
    assert(value.a == 1);
    assert(value.b == 1);
    assert(value.c == 0);
    assert(value.d == 0);

    value.c += 1;
    assert(value.a == 1);
    assert(value.b == 1);
    assert(value.c == 1);
    assert(value.d == 0);

    value.d += 1;
    assert(value.a == 1);
    assert(value.b == 1);
    assert(value.c == 1);
    assert(value.d == 1);
}

const FooArray24Bits = packed struct {
    a: u16,
    b: [2]Foo24Bits,
    c: u16,
};

fn packedArray24Bits() {
    @setFnTest(this);

    comptime {
        assert(@sizeOf([9]Foo24Bits) == 9 * 3);
        assert(@sizeOf(FooArray24Bits) == 2 + 2 * 3 + 2);
    }

    var bytes = []u8{0} ** (@sizeOf(FooArray24Bits) + 1);
    bytes[bytes.len - 1] = 0xaa;
    const ptr = &([]FooArray24Bits)(bytes[0...bytes.len - 1])[0];
    assert(ptr.a == 0);
    assert(ptr.b[0].field == 0);
    assert(ptr.b[1].field == 0);
    assert(ptr.c == 0);

    ptr.a = @maxValue(u16);
    assert(ptr.a == @maxValue(u16));
    assert(ptr.b[0].field == 0);
    assert(ptr.b[1].field == 0);
    assert(ptr.c == 0);

    ptr.b[0].field = @maxValue(u24);
    assert(ptr.a == @maxValue(u16));
    assert(ptr.b[0].field == @maxValue(u24));
    assert(ptr.b[1].field == 0);
    assert(ptr.c == 0);

    ptr.b[1].field = @maxValue(u24);
    assert(ptr.a == @maxValue(u16));
    assert(ptr.b[0].field == @maxValue(u24));
    assert(ptr.b[1].field == @maxValue(u24));
    assert(ptr.c == 0);

    ptr.c = @maxValue(u16);
    assert(ptr.a == @maxValue(u16));
    assert(ptr.b[0].field == @maxValue(u24));
    assert(ptr.b[1].field == @maxValue(u24));
    assert(ptr.c == @maxValue(u16));

    assert(bytes[bytes.len - 1] == 0xaa);
}

const FooStructAligned = packed struct {
    a: u8,
    b: u8,
};

const FooArrayOfAligned = packed struct {
    a: [2]FooStructAligned,
};

fn alignedArrayOfPackedStruct() {
    @setFnTest(this);

    comptime {
        assert(@sizeOf(FooStructAligned) == 2);
        assert(@sizeOf(FooArrayOfAligned) == 2 * 2);
    }

    var bytes = []u8{0xbb} ** @sizeOf(FooArrayOfAligned);
    const ptr = &([]FooArrayOfAligned)(bytes[0...bytes.len])[0];

    assert(ptr.a[0].a == 0xbb);
    assert(ptr.a[0].b == 0xbb);
    assert(ptr.a[1].a == 0xbb);
    assert(ptr.a[1].b == 0xbb);
}
