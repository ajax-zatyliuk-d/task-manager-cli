# Менеджер завдань на C та GLib

Навчальний проєкт: бібліотека менеджера завдань із пріоритетами та
індексом тегів, побудована на контейнерах GLib, і шар консольних команд
поверх неї. Вимоги — у
[docs/glib-containers-task.pdf](docs/glib-containers-task.pdf).

## Стан проєкту

Це **незавершена** версія. Бібліотека (`src/task*`) і обробник команд
(`src/cli.*`) реалізовані та протестовані, але запускного застосунку ще
немає: [main.c](main.c) — заглушка «Hello, World», і `make` його не
збирає. Запускати зараз можна лише тести.

| Можливість | Стан |
| --- | --- |
| Додавання та пошук задачі за ID | готово |
| Теги: `tag`, `untag`, пошук за тегом | готово |
| `top n` за пріоритетом | готово у бібліотеці, обробника CLI ще немає |
| Видалення задачі (`del`) | готово |
| Черга виконання: `enqueue`, `run` | `enqueue` готово; `run` — у бібліотеці, обробника CLI ще немає |
| Історія останніх 10 команд | готово у бібліотеці, обробника `history` ще немає |
| `stats` | готово у бібліотеці, обробника CLI ще немає |
| Обробник команд CLI | є `add`, `del`, `tag`, `untag`, `enqueue`, `find`; бракує `top`, `run`, `history`, `stats` |
| Запускний застосунок (читання stdin, `main.c`) | не реалізовано |

## Вимоги до середовища

- Linux та компілятор C (`cc`/gcc/clang);
- `make` і `pkg-config`;
- бібліотека GLib 2.56 або новіша разом із заголовками
  (Debian/Ubuntu: `sudo apt install build-essential pkg-config libglib2.0-dev`).

Необов'язково, для перевірки пам'яті: `valgrind`.

## Збірка та запуск тестів

```sh
make            # збирає build/test_task, build/test_task_manager, build/test_cli
make test       # збирає за потреби та запускає всі три програми
make -B test    # примусова перебудова та запуск
make clean      # видаляє каталог build
```

Збірка йде з `-Wall -Wextra -Werror`, тож будь-яке попередження є помилкою.
Очікуваний результат — рядки `ok ...` для всіх тестів (зараз 2 у
`test_task`, 16 у `test_task_manager` і 13 у `test_cli`) та код виходу 0:

```sh
make -B test; echo "exit=$?"
```

Тести виводять звіт у форматі TAP. Окремий тест можна запустити так:

```sh
./build/test_task_manager -p /task-manager/untag/valid
```

### Перевірка витоків пам'яті

```sh
valgrind --leak-check=full ./build/test_task
valgrind --leak-check=full ./build/test_task_manager
valgrind --leak-check=full ./build/test_cli
```

Очікується `definitely lost: 0 bytes` та `ERROR SUMMARY: 0 errors`.
Рядки `still reachable` від ініціалізації GLib нормальні. Як альтернатива —
AddressSanitizer:

```sh
cc -Isrc $(pkg-config --cflags glib-2.0) -fsanitize=address,undefined -g \
   tests/test_task_manager.c src/task_manager.c src/task.c \
   -o /tmp/test_task_manager_asan $(pkg-config --libs glib-2.0)
/tmp/test_task_manager_asan
```

## Структура

```text
src/task.h, task.c                 структура Task, створення, копіювання, теги задачі
src/task_manager.h, task_manager.c TaskManager: індекси та операції
src/cli.h, cli.c                   розбір і виконання однієї команди
tests/test_task.c                  тести Task
tests/test_task_manager.c          тести TaskManager
tests/test_cli.c                   тести обробника команд
main.c                             заглушка (поки не використовується)
Makefile                           збірка й запуск тестів
docs/glib-containers-task.pdf      вихідні вимоги
```

Три тестові програми мають власні `main()` і збираються окремо.

## Публічний API

Усі функції оголошено в [src/task_manager.h](src/task_manager.h). Параметр
`GError **error` може бути `NULL`; коли функція повертає `FALSE` або `NULL`
через помилку, вона встановлює `GError` у домені `TM_ERROR`.

| Функція | Призначення |
| --- | --- |
| `task_manager_new` / `task_manager_free` | створення та знищення менеджера (`free(NULL)` дозволений) |
| `task_manager_add_task` | додає задачу, повертає ID від 1; пріоритет поза 1..10 — `TM_ERROR_INVALID_PRIORITY` |
| `task_manager_find_task_by_id` | позичений `const Task *` або `NULL` з `TM_ERROR_TASK_NOT_FOUND` |
| `task_manager_delete_task` | видаляє задачу з усіх індексів |
| `task_manager_tag_task` / `_untag_task` | додає/прибирає тег; тег — `[a-z0-9_]`, без `#` |
| `task_manager_find_tasks_by_tag` | новий масив задач тегу; для валідного тегу без збігів — порожній (`len == 0`), для недопустимого — `NULL` і `TM_ERROR_INVALID_TAG` |
| `task_manager_top_tasks` | новий масив `n` задач з найвищим пріоритетом |
| `task_manager_enqueue_task` | ставить задачу в кінець черги виконання |
| `task_manager_run_next` | бере задачу з голови черги, повертає її копію й видаляє оригінал; `NULL` — черга порожня |
| `task_manager_record_command` / `_get_history` | записує команду та повертає копію історії останніх 10 команд |
| `task_manager_get_stats` | кількість задач, унікальних тегів і записів у черзі (за значенням) |

Правила володіння:

- `find_task_by_id` повертає позичений `const Task *`: його не звільняють;
- `find_tasks_by_tag` і `top_tasks` повертають новий `GPtrArray`, який
  викликач звільняє через `g_ptr_array_unref()`, також порожній результат.
  Масив є копією вказівників, тож зміна його складу не чіпає індекс
  менеджера. Елементи — позичені `Task *`: ними володіє менеджер, і після
  видалення задачі відповідні вказівники недійсні;
- `run_next` повертає нову копію `Task`, яку звільняють через `task_free()`;
- `get_history` повертає новий `GPtrArray` з копіями рядків.

### Приклад: пошук за тегом

```c
GError *error = NULL;
TaskManager *manager = task_manager_new();

guint id = task_manager_add_task(manager, "buy milk", 5, &error);
task_manager_tag_task(manager, id, "home", &error);

GPtrArray *found = task_manager_find_tasks_by_tag(manager, "home", &error);
if (found == NULL) {
    /* недопустимий тег: error містить TM_ERROR_INVALID_TAG */
    g_clear_error(&error);
} else {
    for (guint i = 0; i < found->len; ++i) {
        const Task *task = g_ptr_array_index(found, i);
        g_print("%u %s\n", task->id, task->title);
    }
    g_ptr_array_unref(found);   /* потрібно й для порожнього результату */
}

task_manager_free(manager);
```

Не зберігайте вказівники з `found` після `task_manager_delete_task()` або
`task_manager_run_next()` для відповідної задачі.

## Команди CLI

Обробник `cli_execute_line()` приймає один рядок. Успішний результат
додається до `GString` із кінцевим `\n`, помилка повертається через
`GError` (повідомлення без `\n`), `quit` дає статус `CLI_QUIT`.

| Команда | Результат |
| --- | --- |
| `add <priority> <title> [#tag ...]` | `added task <id>` |
| `del <id>` | `deleted task <id>` |
| `tag <id> <#tag>` / `untag <id> <#tag>` | підтвердження зміни |
| `enqueue <id>` | підтвердження |
| `find <#tag>` | рядок `id priority title` на кожну задачу |
| `quit` | завершення |

Ще не реалізовано в CLI: `top <n>`, `run`, `history`, `stats`.
Помилки (невідомий ID, поганий пріоритет чи тег, невідома команда)
повертаються через `GError`; друкувати їх як `error: ...` буде `main`.

## Чому саме ці контейнери

`TaskManager` не має глобального стану, видає ID та узгоджує всі індекси.
Єдиний власник задач — `tasks_by_id`; інші структури лише позичають
вказівники `Task *`, тому під час видалення задачу спершу прибирають зі
всіх індексів і лише потім звільняють.

| Поле | Контейнер | Навіщо |
| --- | --- | --- |
| `tasks_by_id` | `GHashTable`, ID → `Task *` (`g_direct_hash`) | пошук за ID за O(1); єдиний власник задач |
| `tasks_by_tag` | `GHashTable`, рядок тегу → `GPtrArray` з `Task *` | зворотний індекс «тег → задачі»; масив дозволяє швидко додавати та читати елементи за порядком |
| `unique_tags` | `GHashTable` як множина (`g_hash_table_add`) | набір усіх тегів, що використовуються; логічно дублює ключі `tasks_by_tag`, залишений свідомо за вимогою завдання |
| `tasks_sorted` | `GSequence` з функцією порівняння | порядок за пріоритетом (більший першим), потім старіша задача, потім менший ID; вставка та пошук за O(log n), `top n` читає перші n елементів |
| `run_queue` | `GQueue` | черга виконання FIFO |
| `command_history` | `GQueue` з копіями рядків | останні 10 команд; одинадцята витісняє найстарішу |
| `Task.tags` | `GPtrArray` із `g_free` | невеликий список тегів задачі з автоматичним звільненням рядків |

Чому не `GList` для індексу тегів: пошук елемента й видалення за
індексом у масиві простіші, а порядок і кількість доступні за O(1). Чому
`GSequence`, а не відсортований масив: вставка в середину масиву коштує
O(n), у `GSequence` — O(log n).

## Відомі обмеження

- Переповнення лічильника ID (`guint`) не обробляється.
- Порядок за часом створення не відрізнити в тестах від порядку за ID:
  обидва ростуть разом.
- Однопоточний код; GObject, `GMainLoop` та робочий потік заплановані
  як окремий етап.
