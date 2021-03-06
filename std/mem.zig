const assert = @import("debug.zig").assert;
const math = @import("math.zig");
const os = @import("os.zig");
const io = @import("io.zig");

pub const Cmp = math.Cmp;

error NoMem;

pub type Context = u8;
pub const Allocator = struct {
    allocFn: fn (self: &Allocator, n: usize) -> %[]u8,
    reallocFn: fn (self: &Allocator, old_mem: []u8, new_size: usize) -> %[]u8,
    freeFn: fn (self: &Allocator, mem: []u8),
    context: ?&Context,

    /// Aborts the program if an allocation fails.
    fn checkedAlloc(self: &Allocator, comptime T: type, n: usize) -> []T {
        alloc(self, T, n) %% |err| {
            // TODO var args printf
            %%io.stderr.write("allocation failure: ");
            %%io.stderr.write(@errorName(err));
            %%io.stderr.printf("\n");
            os.abort()
        }
    }

    fn alloc(self: &Allocator, comptime T: type, n: usize) -> %[]T {
        const byte_count = %return math.mulOverflow(usize, @sizeOf(T), n);
        ([]T)(%return self.allocFn(self, byte_count))
    }

    fn realloc(self: &Allocator, comptime T: type, old_mem: []T, n: usize) -> %[]T {
        const byte_count = %return math.mulOverflow(usize, @sizeOf(T), n);
        ([]T)(%return self.reallocFn(self, ([]u8)(old_mem), byte_count))
    }

    fn free(self: &Allocator, mem: var) {
        self.freeFn(self, ([]u8)(mem));
    }
};

/// Copy all of source into dest at position 0.
/// dest.len must be >= source.len.
pub fn copy(comptime T: type, dest: []T, source: []const T) {
    // TODO instead of manually doing this check for the whole array
    // and turning off debug safety, the compiler should detect loops like
    // this and automatically omit safety checks for loops
    @setDebugSafety(this, false);
    assert(dest.len >= source.len);
    for (source) |s, i| dest[i] = s;
}

pub fn set(comptime T: type, dest: []T, value: T) {
    for (dest) |*d| *d = value;
}

/// Return < 0, == 0, or > 0 if memory a is less than, equal to, or greater than,
/// memory b, respectively.
pub fn cmp(comptime T: type, a: []const T, b: []const T) -> Cmp {
    const n = math.min(a.len, b.len);
    var i: usize = 0;
    while (i < n; i += 1) {
        if (a[i] == b[i]) continue;
        return if (a[i] > b[i]) Cmp.Greater else if (a[i] < b[i]) Cmp.Less else Cmp.Equal;
    }

    return if (a.len > b.len) Cmp.Greater else if (a.len < b.len) Cmp.Less else Cmp.Equal;
}

/// Compares two slices and returns whether they are equal.
pub fn eql(comptime T: type, a: []const T, b: []const T) -> bool {
    if (a.len != b.len) return false;
    for (a) |item, index| {
        if (b[index] != item) return false;
    }
    return true;
}

/// Reads an integer from memory with size equal to bytes.len.
/// T specifies the return type, which must be large enough to store
/// the result.
pub fn readInt(bytes: []const u8, comptime T: type, big_endian: bool) -> T {
    var result: T = 0;
    if (big_endian) {
        for (bytes) |b| {
            result = (result << 8) | b;
        }
    } else {
        for (bytes) |b, index| {
            result = result | (T(b) << T(index * 8));
        }
    }
    return result;
}

/// Writes an integer to memory with size equal to bytes.len. Pads with zeroes
/// to fill the entire buffer provided.
/// value must be an integer.
pub fn writeInt(buf: []u8, value: var, big_endian: bool) {
    const uint = @intType(false, @typeOf(value).bit_count);
    var bits = @truncate(uint, value);
    if (big_endian) {
        var index: usize = buf.len;
        while (index != 0) {
            index -= 1;

            buf[index] = @truncate(u8, bits);
            bits >>= 8;
        }
    } else {
        for (buf) |*b| {
            *b = @truncate(u8, bits);
            bits >>= 8;
        }
    }
    assert(bits == 0);
}

fn testStringEquality() {
    @setFnTest(this);

    assert(eql(u8, "abcd", "abcd"));
    assert(!eql(u8, "abcdef", "abZdef"));
    assert(!eql(u8, "abcdefg", "abcdef"));
}

fn testReadInt() {
    @setFnTest(this);

    testReadIntImpl();
    comptime testReadIntImpl();
}
fn testReadIntImpl() {
    {
        const bytes = []u8{ 0x12, 0x34, 0x56, 0x78 };
        assert(readInt(bytes, u32, true) == 0x12345678);
        assert(readInt(bytes, u32, false) == 0x78563412);
    }
    {
        const buf = []u8{0x00, 0x00, 0x12, 0x34};
        const answer = readInt(buf, u64, true);
        assert(answer == 0x00001234);
    }
    {
        const buf = []u8{0x12, 0x34, 0x00, 0x00};
        const answer = readInt(buf, u64, false);
        assert(answer == 0x00003412);
    }
}

fn testWriteInt() {
    @setFnTest(this);

    testWriteIntImpl();
    comptime testWriteIntImpl();
}
fn testWriteIntImpl() {
    var bytes: [4]u8 = undefined;

    writeInt(bytes[0...], u32(0x12345678), true);
    assert(eql(u8, bytes, []u8{ 0x12, 0x34, 0x56, 0x78 }));

    writeInt(bytes[0...], u32(0x78563412), false);
    assert(eql(u8, bytes, []u8{ 0x12, 0x34, 0x56, 0x78 }));

    writeInt(bytes[0...], u16(0x1234), true);
    assert(eql(u8, bytes, []u8{ 0x00, 0x00, 0x12, 0x34 }));

    writeInt(bytes[0...], u16(0x1234), false);
    assert(eql(u8, bytes, []u8{ 0x34, 0x12, 0x00, 0x00 }));
}
