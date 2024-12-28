#ifndef THREAD_SAFETY_ANALYSIS_MUTEX_H
#define THREAD_SAFETY_ANALYSIS_MUTEX_H

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x) // no-op
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define RELEASE_GENERIC(...) THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// Defines an annotated interface for mutexes.
// These methods can be implemented to use any internal mutex implementation.
// class CAPABILITY("mutex") Mutex
// {
//   public:
//     // Acquire/lock this mutex exclusively.  Only one thread can have exclusive
//     // access at any one time.  Write operations to guarded data require an
//     // exclusive lock.
//     void Lock() ACQUIRE();

//     // Acquire/lock this mutex for read operations, which require only a shared
//     // lock.  This assumes a multiple-reader, single writer semantics.  Multiple
//     // threads may acquire the mutex simultaneously as readers, but a writer
//     // must wait for all of them to release the mutex before it can acquire it
//     // exclusively.
//     void ReaderLock() ACQUIRE_SHARED();

//     // Release/unlock an exclusive mutex.
//     void Unlock() RELEASE();

//     // Release/unlock a shared mutex.
//     void ReaderUnlock() RELEASE_SHARED();

//     // Generic unlock, can unlock exclusive and shared mutexes.
//     void GenericUnlock() RELEASE_GENERIC();

//     // Try to acquire the mutex.  Returns true on success, and false on failure.
//     bool TryLock() TRY_ACQUIRE(true);

//     // Try to acquire the mutex for read operations.
//     bool ReaderTryLock() TRY_ACQUIRE_SHARED(true);

//     // Assert that this mutex is currently held by the calling thread.
//     void AssertHeld() ASSERT_CAPABILITY(this);

//     // Assert that is mutex is currently held for read operations.
//     void AssertReaderHeld() ASSERT_SHARED_CAPABILITY(this);

//     // For negative capabilities.
//     const Mutex &operator!() const
//     {
//         return *this;
//     }
// };

// // Tag types for selecting a constructor.
// struct adopt_lock_t
// {
// } inline constexpr adopt_lock = {};
// struct defer_lock_t
// {
// } inline constexpr defer_lock = {};
// struct shared_lock_t
// {
// } inline constexpr shared_lock = {};

// // MutexLocker is an RAII class that acquires a mutex in its constructor, and
// // releases it in its destructor.
// class SCOPED_CAPABILITY MutexLocker
// {
//   private:
//     Mutex *mut;
//     bool   locked;

//   public:
//     // Acquire mu, implicitly acquire *this and associate it with mu.
//     MutexLocker(Mutex *mu) ACQUIRE(mu)
//         : mut(mu),
//           locked(true)
//     {
//         mu->Lock();
//     }

//     // Assume mu is held, implicitly acquire *this and associate it with mu.
//     MutexLocker(Mutex *mu, adopt_lock_t) REQUIRES(mu)
//         : mut(mu),
//           locked(true)
//     {
//     }

//     // Acquire mu in shared mode, implicitly acquire *this and associate it with mu.
//     MutexLocker(Mutex *mu, shared_lock_t) ACQUIRE_SHARED(mu)
//         : mut(mu),
//           locked(true)
//     {
//         mu->ReaderLock();
//     }

//     // Assume mu is held in shared mode, implicitly acquire *this and associate it with mu.
//     MutexLocker(Mutex *mu, adopt_lock_t, shared_lock_t) REQUIRES_SHARED(mu)
//         : mut(mu),
//           locked(true)
//     {
//     }

//     // Assume mu is not held, implicitly acquire *this and associate it with mu.
//     MutexLocker(Mutex *mu, defer_lock_t) EXCLUDES(mu)
//         : mut(mu),
//           locked(false)
//     {
//     }

//     // Same as constructors, but without tag types. (Requires C++17 copy elision.)
//     static MutexLocker Lock(Mutex *mu) ACQUIRE(mu)
//     {
//         return MutexLocker(mu);
//     }

//     static MutexLocker Adopt(Mutex *mu) REQUIRES(mu)
//     {
//         return MutexLocker(mu, adopt_lock);
//     }

//     static MutexLocker ReaderLock(Mutex *mu) ACQUIRE_SHARED(mu)
//     {
//         return MutexLocker(mu, shared_lock);
//     }

//     static MutexLocker AdoptReaderLock(Mutex *mu) REQUIRES_SHARED(mu)
//     {
//         return MutexLocker(mu, adopt_lock, shared_lock);
//     }

//     static MutexLocker DeferLock(Mutex *mu) EXCLUDES(mu)
//     {
//         return MutexLocker(mu, defer_lock);
//     }

//     // Release *this and all associated mutexes, if they are still held.
//     // There is no warning if the scope was already unlocked before.
//     ~MutexLocker() RELEASE()
//     {
//         if (locked)
//             mut->GenericUnlock();
//     }

//     // Acquire all associated mutexes exclusively.
//     void Lock() ACQUIRE()
//     {
//         mut->Lock();
//         locked = true;
//     }

//     // Try to acquire all associated mutexes exclusively.
//     bool TryLock() TRY_ACQUIRE(true)
//     {
//         return locked = mut->TryLock();
//     }

//     // Acquire all associated mutexes in shared mode.
//     void ReaderLock() ACQUIRE_SHARED()
//     {
//         mut->ReaderLock();
//         locked = true;
//     }

//     // Try to acquire all associated mutexes in shared mode.
//     bool ReaderTryLock() TRY_ACQUIRE_SHARED(true)
//     {
//         return locked = mut->ReaderTryLock();
//     }

//     // Release all associated mutexes. Warn on double unlock.
//     void Unlock() RELEASE()
//     {
//         mut->Unlock();
//         locked = false;
//     }

//     // Release all associated mutexes. Warn on double unlock.
//     void ReaderUnlock() RELEASE()
//     {
//         mut->ReaderUnlock();
//         locked = false;
//     }
// };

#ifdef USE_LOCK_STYLE_THREAD_SAFETY_ATTRIBUTES
// The original version of thread safety analysis the following attribute
// definitions.  These use a lock-based terminology.  They are still in use
// by existing thread safety code, and will continue to be supported.

// Deprecated.
#define PT_GUARDED_VAR THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_var)

// Deprecated.
#define GUARDED_VAR THREAD_ANNOTATION_ATTRIBUTE__(guarded_var)

// Replaced by REQUIRES
#define EXCLUSIVE_LOCKS_REQUIRED(...) THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))

// Replaced by REQUIRES_SHARED
#define SHARED_LOCKS_REQUIRED(...) THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

// Replaced by CAPABILITY
#define LOCKABLE THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// Replaced by SCOPED_CAPABILITY
#define SCOPED_LOCKABLE THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

// Replaced by ACQUIRE
#define EXCLUSIVE_LOCK_FUNCTION(...) THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

// Replaced by ACQUIRE_SHARED
#define SHARED_LOCK_FUNCTION(...) THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

// Replaced by RELEASE and RELEASE_SHARED
#define UNLOCK_FUNCTION(...) THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

// Replaced by TRY_ACQUIRE
#define EXCLUSIVE_TRYLOCK_FUNCTION(...) THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

// Replaced by TRY_ACQUIRE_SHARED
#define SHARED_TRYLOCK_FUNCTION(...) THREAD_ANNOTATION_ATTRIBUTE__(shared_trylock_function(__VA_ARGS__))

// Replaced by ASSERT_CAPABILITY
#define ASSERT_EXCLUSIVE_LOCK(...) THREAD_ANNOTATION_ATTRIBUTE__(assert_exclusive_lock(__VA_ARGS__))

// Replaced by ASSERT_SHARED_CAPABILITY
#define ASSERT_SHARED_LOCK(...) THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_lock(__VA_ARGS__))

// Replaced by EXCLUDE_CAPABILITY.
#define LOCKS_EXCLUDED(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

// Replaced by RETURN_CAPABILITY
#define LOCK_RETURNED(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#endif // USE_LOCK_STYLE_THREAD_SAFETY_ATTRIBUTES

#endif // THREAD_SAFETY_ANALYSIS_MUTEX_H