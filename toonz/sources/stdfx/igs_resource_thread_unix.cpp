#include <stdexcept> // std::domain_error()
#include "igs_resource_msg_from_err.h"
#include "igs_resource_thread.h"

//--------------------------------------------------------------------
// pthread_t = unsigned long int(rhel4)
/*
	state が
	PTHREAD_CREATE_JOINABLE なら、pthread_join()を呼ぶこと。
	PTHREAD_CREATE_DETACHED なら、なにも呼ぶ必要がないが、
		thread終了を知るには自前で仕掛けが必要。
*/
pthread_t igs::resource::thread_run(
	void *(*function)(void *), void *func_arg, const int state // PTHREAD_CREATE_JOINABLE/PTHREAD_CREATE_DETACHED
	)
{
	pthread_attr_t attr;
	if (::pthread_attr_init(&attr)) {
		throw std::domain_error("pthread_attr_init(-)");
	}
	if (::pthread_attr_setdetachstate(&attr, state)) {
		throw std::domain_error("pthread_attr_setdetachstate(-)");
	}

	pthread_t thread_id = 0;
	const int erno = ::pthread_create(
		&(thread_id), &attr, function, func_arg);

	if (0 != erno) {
		throw std::domain_error(igs_resource_msg_from_err(
			"pthread_create(-)", erno));
	}
	return thread_id;
}
/*
const bool igs::resource::thread_was_done(const pthread_t thread_id) {
??????????????????????????????????????????????????????????????????
??????????????????????????????????????????????????????????????????
??????????????????????????????????????????????????????????????????
threadの終了方法を見る関数は見つからない。
実行関数の引数で、終了フラグを立てて、外から感知する方法か。
関数が終了するまでの間のタイムラグがあるが、問題はあるのか???
}
*/
void igs::resource::thread_join(const pthread_t thread_id)
{
	const int erno = ::pthread_join(thread_id, NULL);
	if (0 != erno) {
		throw std::domain_error(igs_resource_msg_from_err(
			"pthread_join(-)", erno));
	}
}
