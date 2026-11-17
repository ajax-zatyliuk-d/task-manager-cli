# Менеджер завдань на C та GLib

Навчальний проєкт: бібліотека менеджера завдань із пріоритетами та
індексом тегів, побудована на контейнерах GLib. Вимоги — у
[docs/glib-containers-task.pdf](docs/glib-containers-task.pdf).

## Стан проєкту

Це **незавершена** версія. Готова та протестована бібліотека
(`src/`), але консольного застосунку ще немає: [main.c](main.c) — заглушка
«Hello, World», і `make` його не збирає. Запускати зараз можна лише тести.

| Можливість | Стан |
| --- | --- |
| Додавання та пошук задачі за ID | готово |
| Теги: `tag`, `untag`, пошук за тегом | готово |
| `top n` за пріоритетом | готово |
| Видалення задачі (`del`) | готово |
| Черга виконання: `enqueue`, `run` | готово |
| Історія останніх 10 команд | не реалізовано |
| `stats` | не реалізовано |
| Консольний CLI (читання команд зі stdin) | не реалізовано |

## Вимоги до середовища

- Linux та компілятор C (`cc`/gcc/clang);
- `make` і `pkg-config`;
- бібліотека GLib 2.56 або новіша разом із заголовками
  (Debian/Ubuntu: `sudo apt install build-essential pkg-config libglib2.0-dev`).

Необов'язково, для перевірки пам'яті: `valgrind`.

## Збірка та запуск тестів

```sh
make            # збирає build/test_task та build/test_task_manager
make test       # збирає за потреби та запускає обидві програми
make -B test    # примусова перебудова та запуск
make clean      # видаляє каталог build
```

Збірка йде з `-Wall -Wextra -Werror`, тож будь-яке попередження є помилкою.
Очікуваний результат — рядки `ok ...` для всіх тестів (зараз 2 у
`test_task` і 14 у `test_task_manager`) та код виходу 0:

```sh
make -B test; echo "exit=$?"
```

Тести виводять звіт у форматі TAP. Окремий тест можна запустити так:

```sh
./build/test_task_manager -p /task-manager/untag/valid
```

### Перевірка витоків пам'яті

```sh
valgrind --leak-check=full ./build/test_task_manager
valgrind --leak-check=full ./build/test_task
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
tests/test_task.c                  тести Task
tests/test_task_manager.c          тести TaskManager
main.c                             заглушка (поки не використовується)
Makefile                           збірка й запуск тестів
docs/glib-containers-task.pdf      вихідні вимоги
```

Дві тестові програми мають власні `main()` і збираються окремо.

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
| `task_manager_find_tasks_by_tag` | позичений масив задач тегу, `NULL`, якщо тегу немає |
| `task_manager_top_tasks` | новий масив `n` задач з найвищим пріоритетом |
| `task_manager_enqueue_task` | ставить задачу в кінець черги виконання |
| `task_manager_run_next` | бере задачу з голови черги, повертає її копію й видаляє оригінал; `NULL` — черга порожня |

Правила володіння:

- результат `find_task_by_id` та `find_tasks_by_tag` позичений: його не
  звільняють, а масив тегу чинний лише до наступної зміни менеджера;
- `top_tasks` повертає новий `GPtrArray`, який звільняють через
  `g_ptr_array_unref()`; самі задачі в ньому позичені;
- `run_next` повертає нову копію `Task`, яку звільняють через `task_free()`.

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
| `Task.tags` | `GPtrArray` із `g_free` | невеликий список тегів задачі з автоматичним звільненням рядків |

Чому не `GList` для індексу тегів: пошук елемента й видалення за
індексом у масиві простіші, а порядок і кількість доступні за O(1). Чому
`GSequence`, а не відсортований масив: вставка в середину масиву коштує
O(n), у `GSequence` — O(log n).

Історія команд (`GQueue` з копіями рядків, ліміт 10) буде додана пізніше.

## Відомі обмеження

- Переповнення лічильника ID (`guint`) не обробляється.
- Порядок за часом створення не відрізнити в тестах від порядку за ID:
  обидва ростуть разом.
- Однопоточний код; GObject, `GMainLoop` та робочий потік заплановані
  як окремий етап.
