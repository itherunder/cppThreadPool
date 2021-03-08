
#include "threadpool.hpp"

#include <iostream>
#include <string>
#include <windows.h>

using namespace std;

void fun1(int slp) {
    cout << " hello, fun1 ! " << this_thread::get_id() << endl;
    if (slp > 0) {
        cout << "============ fun1 sleep " << slp << " =========== " << this_thread::get_id() << endl;
        this_thread::sleep_for(chrono::milliseconds(slp));
        // Sleep(slp);
    }
}

struct gfun {
    int operator()(int n) {
        cout << n << " hello, gfun ! " << this_thread::get_id() << endl;
        return 42;
    }
};

class A {
public:
    static int Afun(int n = 0) {
        cout << n << " hello, Afun ! " << this_thread::get_id() << endl;
        return n;
    }

    static string Bfun(int n, string str, char c) {
        cout << n << " hello, Bfun ! " << str.c_str() << " " << int(c) << " " << this_thread::get_id() << endl;
        return str;
    }
};

int main() {
    try {
        threadpool tp{ 50 };
        A a;
        future<void> ff = tp.commit(fun1, 42);
        future<int> fg = tp.commit(gfun{}, 42);
        future<int> gg = tp.commit(a.Afun, 42); // ide 提示错误？但可以编译运行
        future<string> gh = tp.commit(a.Bfun, 42, "42", 42);
        future<string> fh = tp.commit([]() -> string {
            cout << " hello, fh ! " << this_thread::get_id() << endl;
            return "hello, fh ret!";
        });

        cout << " ======= sleep ======= " << this_thread::get_id() << endl;
        this_thread::sleep_for(chrono::milliseconds(42));
        
        for (int i = 0; i < 50; ++i) {
            tp.commit(fun1, i * 42);
        }

        cout << " ======= commit all ====== " << this_thread::get_id() << " idlsize = " << tp.idlCount() << endl;

        cout << " ======= sleep ======= " << this_thread::get_id() << endl;
        this_thread::sleep_for(chrono::seconds(3));

        ff.get(); // 调用.get()获取返回值会等待线程执行完，获取返回值
        cout << fg.get() << " " << fh.get().c_str() << " " << this_thread::get_id() << endl;
        
        cout << " =======  sleep ========= " << this_thread::get_id() << endl;
		this_thread::sleep_for(chrono::seconds(3));

		cout << " =======  fun1,55 ========= " << this_thread::get_id() << endl;
		tp.commit(fun1, 55).get();    //调用.get()获取返回值会等待线程执行完

		cout << "end... " << this_thread::get_id() << endl;


		threadpool pool(4);
		vector<future<int>> results;

		for (int i = 0; i < 8; ++i) {
			results.emplace_back(
				pool.commit([i] {
					cout << "hello " << i << endl;
					this_thread::sleep_for(chrono::seconds(1));
					cout << "world " << i << endl;
					return i * i;
				})
			);
		}
		cout << " =======  commit all2 ========= " << this_thread::get_id() << endl;

		for (auto && result : results)
			cout << result.get() << ' ';
		cout << endl;
	}
    catch (exception& e) {
	    cout << "some unhappy happened...  " << this_thread::get_id() << e.what() << endl;
    }
    return 0;
}