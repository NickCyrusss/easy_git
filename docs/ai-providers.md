# AI providers / AI 服务商

36 built-in configurations, including regional services and local runtimes. Official interface references checked on **2026-09-08**. This is a maintained preset catalog, not an exhaustive registry of every AI vendor. Availability, model IDs, regions and account permissions can change; the app does not fetch the catalog automatically or verify your account’s access. URLs and model IDs remain editable. Use **Add model** for other compatible services or additional models from one provider. Presets cannot be deleted.

内置 **36** 项配置（包含地域服务和本地运行环境），官方接口资料核对日期为 **2026-09-08**。这是可维护的预设目录，无法保证囊括全球所有现存厂商；服务、模型、地域及账户权限会变化，应用不会自动拉取目录或验证账户权限。地址和模型 ID 可编辑；其他兼容服务或同厂商的多个模型可通过 **Add model** 添加。预设项不可删除。

| Provider / 服务商（官方资料） | Default format / 默认格式 | Base URL | Default model / 默认模型 |
| --- | --- | --- | --- |
| [DeepSeek](https://api-docs.deepseek.com/) | OpenAI | `https://api.deepseek.com` | `deepseek-v4-flash` |
| [Kimi](https://platform.kimi.com/docs/guide/kimi-k2-6-quickstart) | OpenAI | `https://api.moonshot.cn/v1` | `kimi-k2.6` |
| [Qwen](https://help.aliyun.com/zh/model-studio/model-calling-in-sub-workspace) | OpenAI | `https://dashscope.aliyuncs.com/compatible-mode/v1` | `qwen-plus` |
| [Doubao](https://www.volcengine.com/docs/82379/1494384) | OpenAI | `https://ark.cn-beijing.volces.com/api/v3` | `doubao-seed-2-0-lite-260215` |
| [OpenAI](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create) | OpenAI | `https://api.openai.com/v1` | `gpt-4.1-mini` |
| [Anthropic](https://platform.claude.com/docs/en/api/messages/create) | Anthropic | `https://api.anthropic.com/v1` | `claude-haiku-4-5-20251001` |
| [Google Gemini](https://ai.google.dev/gemini-api/docs/openai) | OpenAI | `https://generativelanguage.googleapis.com/v1beta/openai` | `gemini-2.5-flash` |
| [Zhipu GLM](https://docs.bigmodel.cn/cn/guide/models/text/glm-5) | OpenAI | `https://open.bigmodel.cn/api/paas/v4` | `glm-5` |
| [Z.ai](https://docs.z.ai/devpack/quick-start) | OpenAI | `https://api.z.ai/api/paas/v4` | `glm-5` |
| [MiniMax](https://platform.minimaxi.com/docs/api-reference/models/anthropic/list-models) | Anthropic | `https://api.minimaxi.com/anthropic/v1` | `MiniMax-M2.7` |
| [MiniMax International](https://platform.minimax.io/docs/api-reference/text-anthropic-api) | Anthropic | `https://api.minimax.io/anthropic/v1` | `MiniMax-M2.7` |
| [Baidu Qianfan](https://cloud.baidu.com/doc/qianfan-docs/s/9m95lyyhm) | OpenAI | `https://qianfan.bj.baidubce.com/v2` | `ernie-4.5-turbo-128k` |
| [Baichuan](https://platform.baichuan-ai.com/docs/api) | OpenAI | `https://api.baichuan-ai.com/v1` | Enter your model ID / 填写可用模型 ID |
| [Tencent Hunyuan](https://cloud.tencent.com/document/product/1729/111007) | OpenAI | `https://api.hunyuan.cloud.tencent.com/v1` | `hunyuan-turbos-latest` |
| [iFlytek Spark](https://www.xfyun.cn/doc/spark/HTTP%E8%B0%83%E7%94%A8%E6%96%87%E6%A1%A3.html) | OpenAI | `https://spark-api-open.xf-yun.com/v1` | `generalv3.5` |
| [StepFun](https://platform.stepfun.com/docs/zh/quickstart/overview) | OpenAI | `https://api.stepfun.com/v1` | `step-3.5-flash` |
| [SiliconFlow](https://docs.siliconflow.cn/docs/userguide/quickstart) | OpenAI | `https://api.siliconflow.cn/v1` | `Pro/deepseek-ai/DeepSeek-R1` |
| [ModelScope](https://www.modelscope.cn/learn/5697) | OpenAI | `https://api-inference.modelscope.cn/v1` | `Qwen/Qwen3.5-35B-A3B` |
| [xAI](https://docs.x.ai/developers/model-capabilities/text/comparison) | OpenAI | `https://api.x.ai/v1` | `grok-4.6` |
| [Mistral](https://docs.mistral.ai/getting-started/quickstarts/studio/activate-and-generate-api-key) | OpenAI | `https://api.mistral.ai/v1` | `mistral-small-latest` |
| [Cohere](https://docs.cohere.com/docs/compatibility-api) | OpenAI | `https://api.cohere.ai/compatibility/v1` | `command-a-plus-05-2026` |
| [Perplexity](https://docs.perplexity.ai/docs/sonar/openai-compatibility) | OpenAI | `https://api.perplexity.ai` | `sonar` |
| [Groq](https://console.groq.com/docs/models) | OpenAI | `https://api.groq.com/openai/v1` | `openai/gpt-oss-120b` |
| [Cerebras](https://inference-docs.cerebras.ai/models/overview) | OpenAI | `https://api.cerebras.ai/v1` | `gpt-oss-120b` |
| [Together AI](https://docs.together.ai/docs/inference/openai-compatibility) | OpenAI | `https://api.together.ai/v1` | `openai/gpt-oss-20b` |
| [Fireworks AI](https://docs.fireworks.ai/tools-sdks/openai-compatibility) | OpenAI | `https://api.fireworks.ai/inference/v1` | `accounts/fireworks/models/llama-v3p1-8b-instruct` |
| [NVIDIA NIM](https://docs.api.nvidia.com/nim/reference/meta-llama-3_3-70b-instruct-infer) | OpenAI | `https://integrate.api.nvidia.com/v1` | `meta/llama-3.3-70b-instruct` |
| [OpenRouter](https://openrouter.ai/docs/quickstart) | OpenAI | `https://openrouter.ai/api/v1` | `openai/gpt-4.1-mini` |
| [Hugging Face](https://huggingface.co/docs/inference-providers/index) | OpenAI | `https://router.huggingface.co/v1` | `openai/gpt-oss-120b` |
| [DeepInfra](https://docs.deepinfra.com/) | OpenAI | `https://api.deepinfra.com/v1/openai` | `deepseek-ai/DeepSeek-V3` |
| [SambaNova](https://docs.sambanova.ai/docs/en/get-started/api-keys-urls) | OpenAI | `https://api.sambanova.ai/v1` | Enter your model ID / 填写可用模型 ID |
| [Novita AI](https://novita.ai/docs/guides/openai-agents-sdk) | OpenAI | `https://api.novita.ai/openai` | Enter your model ID / 填写可用模型 ID |
| [Azure OpenAI](https://learn.microsoft.com/en-us/azure/ai-services/openai/supported-languages) | OpenAI | Your resource URL / 账户资源地址 | Enter your model ID / 填写可用模型 ID |
| [Amazon Bedrock](https://docs.aws.amazon.com/bedrock/latest/userguide/model-parameters-openai.html) | OpenAI | `https://bedrock-runtime.us-west-2.amazonaws.com/openai/v1` | `openai.gpt-oss-20b-1:0` |
| [Ollama](https://docs.ollama.com/api/openai-compatibility) | OpenAI | `http://127.0.0.1:11434/v1` | Enter your model ID / 填写可用模型 ID |
| [LM Studio](https://lmstudio.ai/docs/developer/openai-compat) | OpenAI | `http://127.0.0.1:1234/v1` | Enter your model ID / 填写可用模型 ID |

## Setup / 配置说明

- **OpenAI** means Chat Completions, not Responses. It sends `Authorization: Bearer`, system/user messages and the selected token limit field. OpenAI and Azure presets default to `max_completion_tokens`; compatible services usually use `max_tokens`. [OpenAI parameter reference](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create).
- **Anthropic** means Messages: `x-api-key`, `anthropic-version: 2023-06-01`, a top-level `system`, `messages`, and `max_tokens`. Only text blocks are used for the commit draft. [Anthropic reference](https://platform.claude.com/docs/en/api/messages/create).
- Both formats accept base URLs or complete endpoints. Anthropic adds `/v1/messages` (or `/messages` when the base already ends in `/v1`); OpenAI adds `/chat/completions`. A matching full endpoint is kept intact. Switching format replaces the old endpoint suffix, but provider-specific base paths must be set according to the provider’s docs.
- Azure requires your own resource URL ending in `/openai/v1` and a deployment name. Bedrock requires a **Bedrock API key**, model access and an appropriate region; it does not use an AWS access-key/secret pair. Spark’s API key field takes the HTTP API **APIPassword**. Local runtimes must already be running with a loaded/installed model. Blank model fields require an ID from your account/server rather than an assumed shared default. See each provider’s linked documentation above.
- New custom models start with blank URL, model, key and proxy; they do not copy another profile’s credentials. Names must be unique (ASCII case-insensitive), 1–80 UTF-8 bytes, without control characters or `#`. Up to 100 saved configurations are supported. Delete confirmation modifies the settings draft; **Save settings** persists it, **Cancel** undoes it. Removing the active custom model selects DeepSeek and restores its saved settings.
- Old `Custom` profiles migrate to `Imported model` (with a numeric suffix on collision). Files without a format retain **OpenAI**, the protocol used by older versions. All settings, including format and token field, stay independent per model in `~/.easy_git`; proxy remains blank by default.

- **OpenAI** 使用 Chat Completions（并非 Responses），采用 Bearer 认证，按所选 token 字段发送；OpenAI/Azure 默认 `max_completion_tokens`，多数兼容服务默认 `max_tokens`。
- **Anthropic** 使用 Messages，采用 `x-api-key`、版本头、顶层 system 和 `max_tokens`，仅取文本块生成提交草稿。基础地址和完整端点均可填写，切换协议后需检查厂商专用基础路径是否匹配。
- Azure 填写自己的资源地址及部署名；Bedrock 使用专门的 API key，按开通地域调整地址；讯飞星火填 HTTP API 的 APIPassword。本地服务需先启动并加载/安装模型；留空的模型 ID 应根据账户或本机实际可用模型填写。
- 新自定义模型的地址、模型 ID、密钥和代理均为空，不继承其他配置的凭据。名称需唯一（ASCII 不区分大小写）、1–80 UTF-8 字节，不含控制字符或 `#`；最多保存 100 项。删除当前自定义模型后恢复 DeepSeek 配置。**Save settings** 保存全部修改，**Cancel** 撤销本次增删改。
- 旧 Custom 自动迁移为 Imported model，重名时增加数字后缀；未指定格式的旧配置继续使用 OpenAI。所有模型独立持久化到 `~/.easy_git`，默认代理仍为空。

## Validation / 验证范围

Automated tests use a local HTTP server to check real request paths, headers, JSON bodies, response parsing, errors, cancellation and staged-index protection for both formats. No paid provider requests or account-access checks are performed by the test suite.

自动化测试使用本地 HTTP 服务检查真实请求路径、认证头、JSON、返回解析、错误处理、取消及暂存区过期保护；测试不调用付费服务，也不验证各厂商账户权限。
