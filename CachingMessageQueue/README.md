# toy_caching_message_queue

To block client threads for as short as possible: In-memory caching message queue and its benchmark.

# Benchmark

On my local T420s -- D.K.

```
# Load test, mid-sized messages from several threads.
$ for q in DummyMQ SimpleMQ EfficientMQ ; do \
>   ./build/benchmark \
>   --queue=$q \
>   --average_message_length=1000 \
>   --push_threads=5 \
>   --process_mbps=10 ; \
> done

Benchmarking on 3.00 seconds:
  Queue DummyMQ
  5 threads pushing events at 1.00 MBPS each
  events being processed at 10.00 MBPS
  messages of average size 1000 bytes (0.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:            2013 (0.002 GB, 0.649 MB/s)
Total messages parsed:            2009 (0.002 GB, 0.647 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                  910 (45.21%)
Push time >= 10ms:                 137 (6.81%)
Push time >= 100ms:                 23 (1.14%)

Benchmarking on 3.00 seconds:
  Queue SimpleMQ
  5 threads pushing events at 1.00 MBPS each
  events being processed at 10.00 MBPS
  messages of average size 1000 bytes (0.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:             226 (0.000 GB, 0.072 MB/s)
Total messages parsed:             221 (0.000 GB, 0.070 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                   29 (12.83%)
Push time >= 10ms:                  17 (7.52%)
Push time >= 100ms:                  5 (2.21%)

Benchmarking on 3.00 seconds:
  Queue EfficientMQ
  5 threads pushing events at 1.00 MBPS each
  events being processed at 10.00 MBPS
  messages of average size 1000 bytes (0.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:            2604 (0.003 GB, 0.861 MB/s)
Total messages parsed:            1051 (0.001 GB, 0.342 MB/s)
Total messages dropped:            866 (33.26%)
Push time >= 1ms:                    1 (0.04%)
Push time >= 10ms:                   0 (0.00%)
Push time >= 100ms:                  0 (0.00%)
```

```
# Heavy load test, large messages from many threads.
$ for q in DummyMQ SimpleMQ EfficientMQ ; do \
>   ./build/benchmark \
>   --queue=$q \
>   --average_message_length=1000000 \
>   --push_threads=100 \
>   --process_mbps=100 ; \
> done

Benchmarking on 3.00 seconds:
  Queue DummyMQ
  100 threads pushing events at 1.00 MBPS each
  events being processed at 100.00 MBPS
  messages of average size 1000000 bytes (1.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:             172 (0.181 GB, 60.281 MB/s)
Total messages parsed:              91 (0.054 GB, 17.872 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                  166 (96.51%)
Push time >= 10ms:                 164 (95.35%)
Push time >= 100ms:                148 (86.05%)

Benchmarking on 3.00 seconds:
  Queue SimpleMQ
  100 threads pushing events at 1.00 MBPS each
  events being processed at 100.00 MBPS
  messages of average size 1000000 bytes (1.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:             199 (0.207 GB, 69.163 MB/s)
Total messages parsed:              63 (0.026 GB, 8.609 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                  192 (96.48%)
Push time >= 10ms:                 186 (93.47%)
Push time >= 100ms:                172 (86.43%)

Benchmarking on 3.00 seconds:
  Queue EfficientMQ
  100 threads pushing events at 1.00 MBPS each
  events being processed at 100.00 MBPS
  messages of average size 1000000 bytes (1.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:             289 (0.290 GB, 96.645 MB/s)
Total messages parsed:              67 (0.043 GB, 14.485 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                  190 (65.74%)
Push time >= 10ms:                 168 (58.13%)
Push time >= 100ms:                134 (46.37%)
```

```
# Consumer slow relative to producers, observe produce speed adjusted to the consumer rate and/or messages dropped.
# Need more time and smaller packets, otherwith most of them end up in the circular buffer of EfficientMQ.
$ for q in DummyMQ SimpleMQ EfficientMQ ; do \
  ./build/benchmark \
  --queue=$q \
  --average_message_length=100 \
  --push_threads=4 \
  --push_mbps_per_thread=1 \
  --process_mbps=5 \
  --seconds=15 ; \
done

Benchmarking on 15.00 seconds:
  Queue DummyMQ
  4 threads pushing events at 1.00 MBPS each
  events being processed at 5.00 MBPS
  messages of average size 100 bytes (0.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:           25029 (0.003 GB, 0.167 MB/s)
Total messages parsed:           25027 (0.003 GB, 0.167 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                 1488 (5.95%)
Push time >= 10ms:                 161 (0.64%)
Push time >= 100ms:                 23 (0.09%)

Benchmarking on 15.00 seconds:
  Queue SimpleMQ
  4 threads pushing events at 1.00 MBPS each
  events being processed at 5.00 MBPS
  messages of average size 100 bytes (0.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:           26399 (0.003 GB, 0.176 MB/s)
Total messages parsed:           26200 (0.003 GB, 0.175 MB/s)
Total messages dropped:              0 (0.00%)
Push time >= 1ms:                 1088 (4.12%)
Push time >= 10ms:                 230 (0.87%)
Push time >= 100ms:                 44 (0.17%)

Benchmarking on 15.00 seconds:
  Queue EfficientMQ
  4 threads pushing events at 1.00 MBPS each
  events being processed at 5.00 MBPS
  messages of average size 100 bytes (0.00 MB), with the minimum of 16 bytes (0.00 MB)
Total messages pushed:           47094 (0.005 GB, 0.314 MB/s)
Total messages parsed:           32553 (0.003 GB, 0.216 MB/s)
Total messages dropped:          14541 (30.88%)
Push time >= 1ms:                   70 (0.15%)
Push time >= 10ms:                  11 (0.02%)
Push time >= 100ms:                  0 (0.00%)
```
