#include <stdio.h>
#include <windows.h>
#include <stdint.h>

/*
测试用例：
 1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
10 13 15 16 22 24 27 28 31 39 40 41 42 43 44
 3  8 13 14 15 16 17 25 26 27 31 32 33 34 35
*/

#define TOTAL_PEOPLE 45            // 总人数45
#define LEADER_COUNT 15            // 每个班子必须有15人
#define INITIAL_GROUP_CAPACITY 128 // 全局合法班子数组初始容量

// 定义班子（Group）结构体，保存位表示及15个成员（按升序排列）
typedef struct
{
    uint64_t mask;             // 用位(0/1)表示入选情况，第 i 个人对应 mask 的第 (i-1) 位
    int members[LEADER_COUNT]; // 按升序保存的 15 个编号
} Group;

Group *gGroups = NULL;                  // 全局保存的所有（已存在的）班子，初始班子存入第 0 个位置
int gGroupCount = 0;                    // 计数
int gGroupCap = INITIAL_GROUP_CAPACITY; // 初始班子数组容量
uint64_t gInitialMask = 0;              // 初始班子的位表示

// addGroup：添加新班子到全局数组中（内部用 OpenMP critical 保护）
void addGroup(uint64_t mask, int comb[])
{
#pragma omp critical
    {
        if (gGroupCount >= gGroupCap)
        {
            perror("班子数量超出上限！");
            exit(EXIT_FAILURE);
        }

        gGroups[gGroupCount].mask = mask;
        for (int i = 0; i < LEADER_COUNT; i++)
            gGroups[gGroupCount].members[i] = comb[i];
        gGroupCount++;

        printf("班子[%3d]:", gGroupCount);
        for (int i = 0; i < LEADER_COUNT; i++)
            printf("%2d ", comb[i]);
        printf("\n");
    }
}

// 递归生成 15 人班子组合（按编号递增选择）
void dfs(const int start,     // 下一个候选编号起点
         const int count,     // 当前已选人数
         int comb[],          // 已选成员集合（升序存放）
         const uint64_t mask) // 已选集合的位表示
{
    if (count == LEADER_COUNT)
    {
        // 检查当前组合与所有已存在班子的交集情况，若有交集达到6个或以上，则剪枝返回
        for (int i = 0; i < gGroupCount; i++)
            if (__builtin_popcountll(mask & gGroups[i].mask) >= 6)
                return;
        // 满足条件，调用 addGroup 保存班子（addGroup 内部保护为串行操作）
        addGroup(mask, comb);
        return;
    }
    // 剪枝：若剩余可选择人数不足以凑满 15 人，则返回
    if (TOTAL_PEOPLE - start + 1 < LEADER_COUNT - count)
        return;

    // 检查当前组合与已存在班子的交集，若其中任一班子的交集已达6个，则直接剪枝
    for (int i = 0; i < gGroupCount; i++)
        if (__builtin_popcountll(mask & gGroups[i].mask) >= 6)
            return;

    // 遍历候选编号，从 start 到 TOTAL_PEOPLE
    for (int i = start; i <= TOTAL_PEOPLE; i++)
    {
        // 连续编号检查：若当前已有至少3个成员，并且倒数第3个成员加3恰好等于当前候选编号 i，
        // 则说明这条分支会形成4个连续编号，故直接跳过该候选，剪枝处理
        if (count >= 3 && (comb[count - 3] + 3 == i))
            continue;

        comb[count] = i;
        dfs(i + 1, count + 1, comb, mask | (1ULL << (i - 1)));
    }
}

int main()
{
    // ******************************************************
    // 以下为前端逻辑部分，用于接收 POST 数据
    // ******************************************************

    // 1. 输出HTTP响应头（告知浏览器接下来是纯文本）
    //    如果想用HTML格式返回，可以改成 "Content-type: text/html\n\n"
    printf("Content-type: text/plain\n\n");

    // 2. 获取POST数据长度
    char *len_str = getenv("CONTENT_LENGTH");
    if (!len_str)
    {
        printf("No CONTENT_LENGTH found.\n");
        return 0;
    }
    int content_length = atoi(len_str);
    if (content_length <= 0)
    {
        printf("CONTENT_LENGTH is invalid: %s\n", len_str);
        return 0;
    }

    // 3. 读取POST数据
    char *post_data = (char *)malloc(content_length + 1);
    if (!post_data)
    {
        printf("Memory allocation failed.\n");
        return 0;
    }
    // 初始化
    memset(post_data, 0, content_length + 1);
    // fread 一次性读入所有字节
    fread(post_data, 1, content_length, stdin);

    // 4. 找到 "nums=" 起始位置
    //    假设表单字段名就是 "nums"，大写小写要保持一致
    char *post_start = strstr(post_data, "nums=");
    if (!post_start)
    {
        printf("No 'nums=' found in POST data.\n");
        free(post_data);
        return 0;
    }

    // 跳过 "nums="
    post_start += 5;

    // 5. 将后面这一段按空格拆分出 15 个整数
    //    如果确实只有 15 个，就可以直接循环 15 次；若想更通用，可用 while(token != NULL)。
    int arr[15] = {0};
    int i = 0;
    char *token = strtok(post_start, " ");
    while (token != NULL && i < 15)
    {
        arr[i] = atoi(token);
        token = strtok(NULL, " ");
        i++;
    }

    // 6. 打印解析结果
    printf("You have posted 15 integers:\n");
    for (int j = 0; j < 15; j++)
    {
        printf("%d ", arr[j]);
    }
    printf("\n");

    // ******************************************************
    // 前端逻辑部分结束
    // ******************************************************

    // 记录总运行时间
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    // 设置控制台输出编码为 UTF-8
    SetConsoleOutputCP(CP_UTF8);

    gGroups = malloc(gGroupCap * sizeof(Group));
    if (gGroups == NULL)
    {
        fprintf(stderr, "初始化内存失败！\n");
        return EXIT_FAILURE;
    }

    // 用户输入初始班子：15 个成员（1-45，空格隔开），建议输入时保证升序
    int initGroup[LEADER_COUNT];
    printf("请输入初始班子的 15 个成员编号（1-45，空格隔开）：\n");
    for (int i = 0; i < LEADER_COUNT; i++)
    {
        scanf("%d", &initGroup[i]);
    }

    // 构造初始班子的位表示
    gInitialMask = 0;
    for (int i = 0; i < LEADER_COUNT; i++)
    {
        gInitialMask |= (1ULL << (initGroup[i] - 1));
    }

    // 初始化全局数组，将初始班子先放入（以后所有新加入的班子必须与它交集不超过6）
    gGroupCount = 0;
    addGroup(gInitialMask, initGroup);

    int comb[LEADER_COUNT];
    // 开始从 1 开始递归生成 15 人组合
    dfs(1, 0, comb, 0ULL);

    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
    printf("Total time: %.3f s\n", elapsed);

    system("pause");
    return 0;
}
