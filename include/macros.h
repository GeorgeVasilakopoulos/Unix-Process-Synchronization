#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

#define SEM_OPEN_FAIL_CHECK(arg)			\
{											\
	if(arg == SEM_FAILED){					\
		fprintf(stderr, "sem_open failed");	\
		exit(EXIT_FAILURE);					\
	}										\
}											

#define SEM_UNLINK_FAIL_CHECK(string)		\
{											\
	if(sem_unlink(string)<0){				\
		perror("sem_unlink failure");		\
		exit(1);							\
	}										\
}											

#define SEM_WAIT_FAIL_CHECK(arg)					\
{													\
	if(sem_wait(arg) < 0){							\
		perror("sem_wait failed on parent process");\
		exit(EXIT_FAILURE); 						\
	}												\
}

#define SEM_POST_FAIL_CHECK(arg)					\
{													\
	if(sem_post(arg) < 0){							\
		perror("sem_post failed on parent process");\
		exit(EXIT_FAILURE); 						\
	}												\
}

