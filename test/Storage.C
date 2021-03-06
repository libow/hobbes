
#include <hobbes/hobbes.H>
#include <hobbes/db/file.H>
#include <hobbes/db/series.H>
#include <hobbes/db/signals.H>
#include "test.H"

using namespace hobbes;
static cc& c() { static cc x; return x; }

template <typename T>
  T sum(const T* b, const T* e) {
    T r = T(0);
    while (b != e) {
      r += *b;
      ++b;
    }
    return r;
  }

template <typename T>
  T sum(const array<T>* vs) {
    return sum<T>(vs->data, vs->data + vs->size);
  }

template <typename T>
  void initSeq(array<T>* vs, size_t k, T i, T e) {
    while (i != e) {
      vs->data[k++] = i;
      ++i;
    }
    vs->size = k;
  }

template <typename T>
  T sumFromTo(T s, T e) {
    T n = e - s;
    return ((n*(n+1))/2) - ((s*(s+1))/2);
  }

static bool fileExists(const char* fileName) {
  FILE* fd = fopen(fileName, "r");
  if (fd != NULL) {
    fclose(fd);
    return true;
  } else {
    return false;
  }
}

TEST(Storage, uniqueFilenames) {
  // Create file1
  std::string file1 = uniqueFilename("/tmp/hfiles-unittest", ".log.staging");
  EXPECT_TRUE(fileExists(file1.c_str()));

  // Create file2
  std::string file2;
  try {
    file2 = uniqueFilename("/tmp/hfiles-unittest", ".log.staging");
  }
  catch(...) {
    unlink(file1.c_str());
    throw;
  }
  EXPECT_TRUE(fileExists(file2.c_str()));

  // Rename file2 to file 3
  std::string file3;
  try{
    file3 = moveToUniqueFilename(file2, "/tmp/hfiles-unittest", ".log");
  }
  catch(...) {
    unlink(file2.c_str());
    unlink(file1.c_str());
    throw;
  }
  EXPECT_FALSE(fileExists(file2.c_str()));
  EXPECT_TRUE(fileExists(file3.c_str()));

  // Clean up the files
  unlink(file3.c_str());
  unlink(file1.c_str());
}

static std::string mkFName() {
  return uniqueFilename("/tmp/hdb-unittest", ".db");
}

TEST(Storage, Create) {
  std::string fname = mkFName();
  try {
    // make sure that we can create a file and write some stuff to it
    writer f(fname);

    int n = 1020;
    array<int>* vs = f.define<int>("vs", n);
    EXPECT_TRUE(vs != 0);
    EXPECT_TRUE(vs->size == 0);

    initSeq(vs, 0, 0, n);
    EXPECT_EQ(sum(vs), sumFromTo(0, 1019));

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

DEFINE_STRUCT(SeriesTest,
  (int,                x),
  (double,             y),
  (const array<char>*, z)
);

TEST(Storage, SeriesAPI) {
  std::string fname = mkFName();
  try {
    // make sure that we can write struct data through the series API correctly
    writer f(fname);
    series<SeriesTest> ss(&c(), &f, "series_test");

    for (size_t i = 0; i < 10; ++i) {
      SeriesTest st;
      st.x = i;
      st.y = 3.14159 * ((double)i);
      st.z = makeString("string_" + str::from(i));
      ss(st);
    }

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, Modify) {
  std::string fname = mkFName();
  try {
    // make a file with a sequence in it, which we extend with a second writer, then make sure that the unified view is visible
    writer f0(fname);
    int n = 1020;
    array<int>* vs = f0.define<int>("vs", n);
    initSeq(vs, 0, 0, n / 2);
    EXPECT_EQ(sum(vs), sumFromTo(0, (1020/2) - 1));

    writer f1(fname);
    array<int>* vs2 = f1.lookup<array<int>*>("vs");
    EXPECT_EQ(sum(vs2), sumFromTo(0, (1020/2) - 1));
    initSeq(vs2, n / 2, n / 2, n);

    reader f2(fname);
    array<int>* vs3 = f2.lookup<array<int>*>("vs");
    EXPECT_EQ(sum(vs3), sumFromTo(0, 1019));

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, Inference) {
  cc c;
  EXPECT_EQ(show(c.unsweetenExpression(c.readExpr("newPrim() :: (StoreInto () {a:int,b:[char]} z) => z"))->type()), "{ a:int, b:[char]@? }");
}

TEST(Storage, Scripting) {
  std::string fname = mkFName();
  try {
    static int n = 1020;

    cc wc;
    wc.define("f", "writeFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");
    wc.compileFn<void()>("f.vs <- allocateArray(" + str::from(n) + "L)")();
    array<int>* vs = wc.compileFn<array<int>*()>("load(f.vs)")();
    initSeq(vs, 0, 0, n);

    cc rc;
    rc.define("f", "readFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");
    EXPECT_EQ(rc.compileFn<int()>("sum(load(f.vs))")(), sumFromTo(0, n - 1));

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, Alignment) {
  std::string fname = mkFName();
  try {
    writer f(fname);
    short*        s = f.define<short>("s");
    int*          i = f.define<int>("i");
    double*       d = f.define<double>("d");
    array<int>*   a = f.define<int>("a", 100);
    fileref<int>* r = f.define<fileref<int>>("r");

    EXPECT_EQ(((size_t)s)%sizeof(short),  0);
    EXPECT_EQ(((size_t)i)%sizeof(int),    0);
    EXPECT_EQ(((size_t)d)%sizeof(double), 0);
    EXPECT_EQ(((size_t)a)%sizeof(size_t), 0);
    EXPECT_EQ(((size_t)r)%sizeof(size_t), 0);
    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, GrowAwayFromReader) {
  std::string fname = mkFName();
  try {
    // start the writer with a small array
    cc wc;
    wc.define("f", "writeFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");
    wc.compileFn<void()>("f.vs <- allocateArray(1L)")();
    wc.compileFn<void()>("do { vs = load(f.vs); vs[0] <- 1; unsafeSetLength(vs, 1L); }")();

    // start the reader and bind file variables
    cc rc;
    rc.define("f", "readFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");

    // at this point we've got an array of length 1 that sums to 1
    EXPECT_TRUE(rc.compileFn<bool()>("length(load(f.vs)) == 1 and sum(load(f.vs)) == 1")());

    // now update the file so that it grows away from the reader's current state
    //  (the new array will be filled with 0s automatically)
    size_t nlen = 10000;
    wc.compileFn<void()>("f.vs <- allocateArray(" + str::from(nlen) + "L)")();
    wc.compileFn<void()>("do { vs = load(f.vs); vs[" + str::from(nlen - 1) + "] <- 8675309; unsafeSetLength(vs, " + str::from(nlen) + "L); }")();

    // and make sure that the reader sees this new state correctly
    EXPECT_TRUE(rc.compileFn<bool()>("length(load(f.vs)) == " + str::from(nlen) + " and sum(load(f.vs)) == 8675309")());

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, EarlyStaging) {
  std::string fname = mkFName();
  try {
    cc wc;
    wc.define("f", "writeFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");
    wc.compileFn<void()>("f.vs <- allocateArray(100L)")();

    cc rc;
    rc.define("f", "inputFile :: (LoadFile \"" + fname + "\" x) => x");
    EXPECT_TRUE(rc.compileFn<bool()>("capacity(f.vs) == 100")());

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, ScriptSignals) {
  std::string fname = mkFName();
  try {
    // start the writer with an array
    cc wc;
    wc.define("f",     "writeFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");
    wc.define("pushv", "\\f vs v.do { lvs = load(vs); lvs[length(lvs)] <- v; unsafeSetLength(lvs, length(lvs) + 1); signalUpdate(f); }");
    wc.compileFn<void()>("f.vs <- allocateArray(1000L)")();

    // start a reader with a watch on the writer's array set to increment a couple of local values
    cc rc;
    std::pair<long, int> lensum(0, 0);
    rc.bind("lensum", &lensum);
    rc.define("f", "readFile(\"" + fname + "\") :: ((file) _ {vs:[int]@?})");
    rc.compileFn<void()>("addFileSignal(f.vs, \\_.let _ = lensum.0 <- length(load(f.vs)); _ = lensum.1 <- sum(load(f.vs)) in true)")();

    // now write a few changes and make sure that they're detected
    wc.compileFn<void()>("pushv(f, f.vs, 0)")();
    EXPECT_TRUE(stepEventLoop());
    EXPECT_TRUE(rc.compileFn<bool()>("lensum.0 == 1 and lensum.1 == 0")());

    wc.compileFn<void()>("pushv(f, f.vs, 1)")();
    EXPECT_TRUE(stepEventLoop());
    EXPECT_TRUE(rc.compileFn<bool()>("lensum.0 == 2 and lensum.1 == 1")());

    wc.compileFn<void()>("pushv(f, f.vs, 2)")();
    EXPECT_TRUE(stepEventLoop());
    EXPECT_TRUE(rc.compileFn<bool()>("lensum.0 == 3 and lensum.1 == 3")());

    wc.compileFn<void()>("pushv(f, f.vs, 3)")();
    EXPECT_TRUE(stepEventLoop());
    EXPECT_TRUE(rc.compileFn<bool()>("lensum.0 == 4 and lensum.1 == 6")());

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, CppSignals) {
  std::string fname = mkFName();
  try {
    writer w(fname);
    *w.define<int>("x") = 42;
    w.signalUpdate();

    reader r(fname);
    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, Comprehensions) {
  std::string fname = mkFName();
  try {
    cc wc;
    wc.define("f", "writeFile(\"" + fname + "\") :: ((file) _ {vs:(^x.(()+([int]@?*x@?)))@?})");
    wc.compileFn<void()>("f.vs <- store(roll(|0=()|) :: (^x.(()+([int]@f*x@f))))")();

    // for now, make sure that we can just compile a comprehension over a stored rope
    EXPECT_TRUE(wc.compileFn<bool()>("let _ = [x | x <- f.vs, x > 20] in true")());

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

TEST(Storage, Lift) {
  std::string fname = mkFName();
  try {
    writer w(fname);

    // make sure that type lifting works for arrays of pairs
    auto* xs = w.define< std::pair<int, double> >("xs", 100);
    for (size_t i = 0; i < 100; ++i) {
      xs->data[i].first  = i+1;
      xs->data[i].second = (double)i;
    }
    xs->size = 100;

    cc rc;
    rc.define("f", "inputFile :: (LoadFile \"" + fname + "\" w) => w");
    EXPECT_EQ(rc.compileFn<int()>("sum([x|(x,_)<-f.xs])")(), 5050);

    unlink(fname.c_str());
  } catch (...) {
    unlink(fname.c_str());
    throw;
  }
}

