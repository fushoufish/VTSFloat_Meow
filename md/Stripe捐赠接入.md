# Local-only trial

运行（在项目根目录）：

```powershell
node donation-worker/local-server.mjs
```

打开 http://127.0.0.1:8787/support.html 。默认使用本地模拟收银台，不连接 Stripe，不需要账户，不收钱。模拟成功、取消、金额错误和重复请求均可测试。会话只保存在内存，重启清空。

真实 Stripe 测试：通过本机进程环境变量设置 STRIPE_SECRET_KEY（sk_test_ 开头），重新启动同一命令。不要把密钥写入网页、仓库或聊天；程序拒绝正式密钥。前端仍由本地服务器提供，Worker 使用 Stripe HTTPS API 创建测试 Checkout 会话，昵称只保存为该会话 metadata，邮箱在 Stripe 收银台填写。通过 Stripe 测试付款不代表账户已获准正式收款。

## Cloudflare 与 GitHub Pages

Worker 地址为 https://vtsfloat-meow-support.donation-worker.workers.dev 。GitHub Pages 从 main 分支的 docs 目录发布。Worker 配置在 donation-worker/wrangler.jsonc，包含站点来源、返回地址、限流和 Stripe 正式模式。

在 Cloudflare 控制台的 Worker Settings → Variables and Secrets 添加名为 STRIPE_SECRET_KEY 的 Secret。使用新建的限权正式密钥，允许创建和读取 Checkout Sessions。旧的本地实付测试密钥已在聊天中暴露，应在 Stripe 撤销。新密钥只输入 Cloudflare 的 Secret 表单。

缺少密钥时，网站会显示付款尚未开放。配置完成后，从 GitHub Pages 检查自定义金额能否进入 Stripe Checkout，并核对取消返回和支付确认。若要变更 Worker，运行 npx wrangler deploy --config donation-worker/wrangler.jsonc。

付款结果从后端查询 Stripe 核实，不信任 success URL 参数。不保存捐赠记录或显示芳名榜；如以后要长期记录或处理异步到账，需增加签名验证的 Stripe webhook 和存储。正式上线还需配置反滥用措施和确认 Stripe 账户收款资格。

检查：`node --test donation-worker/worker.test.mjs`
