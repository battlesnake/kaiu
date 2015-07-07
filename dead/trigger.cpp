#include "trigger.h"

namespace mark {

bool Trigger::wait_for()
{
	std::unique_lock<std::mutex> lock(mutex);
	int wait_n = wait_end++;
	auto end_condition = [this, wait_n] {
		return wait_start > wait_n || disabled;
	};
	if (!end_condition()) {
		wait_count++;
		cv.wait(lock, end_condition);
		wait_count--;
	}
	return wait_start > wait_n;
}

void Trigger::trigger_one()
{
	wait_start++;
	cv.notify_all();
}

void Trigger::trigger_n(const int n)
{
	wait_start += n;
	cv.notify_all();
}

void Trigger::trigger_all()
{
	wait_start.exchange(wait_end);
	cv.notify_all();
}

void Trigger::disable()
{
	disabled = true;
	cv.notify_all();
}

}

#ifdef test_trigger
#include <vector>
#include <thread>
#include <stdio.h>

#include <unistd.h>
#include <termios.h>

/* http://stackoverflow.com/a/912796/1156377 */
char getch() {
	char buf = 0;
	struct termios old = {0};
	if (tcgetattr(0, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(0, &buf, 1) < 0)
		perror ("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror ("tcsetattr ~ICANON");
	return (buf);
}

int main(int argc, char *argv[])
{
	mark::Trigger trigger;
	std::vector<std::thread> threads;
	const int nthreads = 3;
	for (int i = 1; i <= nthreads; i++) {
		threads.emplace_back([&trigger, i] {
			trigger.wait_for();
			printf("Triggered thread #%d/%d\n", i, nthreads);
			trigger.wait_for();
			printf("Terminated thread #%d\n", i);
		});
	}
	char c = 0;
	/* Not portable */
	printf("TRIGGER test\n\n"
		"Each thread will terminate after being triggered twice\n\n"
		"\t[1] to trigger one thread\n"
		"\t[2] to trigger one thread\n"
		"\t[a] to trigger all threads\n"
		"\t[q] to quit\n");
	while ((c = getch()) != 'q') {
		if (c == '1') {
			trigger.trigger_one();
		} else if (c == '2') {
			trigger.trigger_n(2);
		} else if (c == 'a') {
			trigger.trigger_all();
		}
	}
	trigger.disable();
	for (auto& thread : threads) {
		thread.join();
	}
	printf("All threads have terminated\n");
	return 0;
}
#endif
