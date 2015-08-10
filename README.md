资源申请和使用情况:

主线程资源: (监听线程)
1. 用于监听的, cm_id (唯一).
2. 用于注册监听异步事件的, cm_channel (唯一).

工作线程资源:
1. 用于注册发送/请求异步时间的, comp_channel ( per thread ).
2. 保护域, pd ( per thread ), 暂时还不知道这个有什么用.
3. 完成队列(可共享), complete queue ( per thread ), send cq 和 recv cq 可以分开来, 也可以共用一个.
4. 共享的发送请求队列, shared request queue. ( per thread ).

PS: 线程内的所有连接都可以共用以上资源

连接相关资源:
1. queue pair, ( per connection ).
2. 内存区域 mr, ( 可以是 一对多 的关系, 还可以注册一次, 重复使用).

PS: 以上两个可能是需要反复申请/注册的

TODO:
1. 理论上一个线程对应一个 comp_channel, 但是由于comp_channel(以及那4个资源)的申请需要设备上下文(device context), 然而这个
东西要等连接请求到来的时候才能确定, 并不能在线程开始的时候就申请, 感觉写起来有点苦笑不得
