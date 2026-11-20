# Передача навчального проєкту

Знімок станом на 2026-11-17. Це контекст передачі, а не доказ поточного
стану після наступних змін. Перечитай код і вимоги перед висновками.
Архітектура та контракти — у CLAUDE.md; запуск і огляд для людини — у
README.md у корені.

## Стартовий промпт для нової розмови

Скопіюй цей блок у Claude Code, відкритий у корені проєкту:

```text
Продовжуй цей проєкт як мій наставник і рев’юер українською.
Код пишу я. Не редагуй файли й не пиши реалізацію без мого прямого
прохання про конкретну зміну. «Перевір», «далі» і «+» означають рев’ю
або продовження навчання, а не дозвіл писати код за мене.

Спочатку прочитай CLAUDE.md, .claude/rules/mentoring.md,
.claude/handoff.md та docs/glib-containers-task.pdf.
Переглянь фактичні файли src/, tests/, CMakeLists.txt і main.c.

Проведи рев’ю поточної реалізації: ./autobuild.sh test (ASan),
./autobuild.sh valgrind і ./autobuild.sh format-check; перевір код
виходу, не фільтруй вивід. Спочатку поведінка, володіння пам’яттю,
GError та зміст тестів. Для кожної проблеми — файл/рядок, сценарій і
пояснення. Файли не змінюй.

Якщо суттєвих проблем немає, продовжуй за розділом «Далі»:
дай одне посильне завдання.
```

## Що реалізовано

- `Task` (`src/task.*`): `task_new`, `task_free`,
  `task_is_tag_existed`, `task_add_tag`, `task_remove_tag`.
- `TaskManager` (`src/task_manager.*`): `tasks_by_id`, `tasks_by_tag`
  (`GPtrArray` з позиченими `Task *`), `unique_tags`, `tasks_sorted`
  (`GSequence`), `run_queue` (`GQueue` з `Task *`), `command_history`
  (`GQueue` з копіями рядків, 10).
- Операції: `add_task`, `find_task_by_id`, `delete_task`, `tag_task`,
  `untag_task`, `find_tasks_by_tag`, `top_tasks`, `enqueue_task`,
  `run_next`, `record_command`, `get_history`, `get_stats`,
  `is_valid_tag`. Спільне видалення — `task_manager_delete_task()`.
- CLI (`src/cli.*`): `cli_execute_line()`, таблиця `commands[]`, усі
  11 команд PDF. `main.c` читає stdin через `GIOChannel`.
- Тести: 2 у `test_task`, 16 у `test_task_manager`, 19 у `test_cli`;
  усі проходять під ASan і valgrind (2026-11-18).
- Збірка: CMake з пресетами `debug` (ASan) і `release`, каталоги
  `../task-manager-cli-<preset>/out/bin`; обгортка `autobuild.sh`
  (configure/build/test/valgrind/clean/format/format-check). Makefile
  прибрано. `.clang-format` взято з mcu_communicator; формат ще не
  застосовано до коду.

## Прийняті рішення, які легко забути

- Теги без `#` у менеджері, лише `[a-z0-9_]`; недопустимі відхиляються
  (не нормалізуються). У CLI тег задається з `#`.
- Повторний tag — `TM_ERROR_DUPLICATE_TAG` (також `#y #y` в одному `add`);
  untag відсутнього тегу — тихий `TRUE`.
- У коді використовуються `g_autoptr` / `g_autofree` / `g_auto(GStrv)`.
- `find_tasks_by_tag` повертає `GPtrArray *` (без `const`): валідний тег
  завжди дає новий масив (порожній, якщо збігів немає), недопустимий —
  `NULL` і `TM_ERROR_INVALID_TAG`. Викликач звільняє через
  `g_ptr_array_unref()`, елементи — позичені `Task *`.
- Дублікати в `run_queue` дозволені; порожня черга для `run` — `NULL`
  без помилки; `run` повертає копію `Task`, яку звільняють `task_free()`.
- `tasks_by_tag` та `run_queue` тримають `Task *`, а не ID (рішення
  користувача заради відповідності PDF; див. обґрунтування в CLAUDE.md).
- Історію записує CLI: кожен непорожній рядок до виконання, включно з
  невдалими командами; `quit` і `history` не записуються.
- Атомарність `add`: теги перевіряються через `task_manager_is_valid_tag`
  до створення задачі.
- Вивід CLI завершується `\n`, помилки `GError` — без `\n`.

## Далі

- Вимоги PDF закрито, включно з дубльованим тегом (2026-11-18).
- Застосувати `.clang-format` до коду окремим комітом (після рішення щодо
  `DerivePointerAlignment`), потім тримати `format-check` зеленим.
- Оновити README: CMake, `autobuild.sh`, запуск застосунку, приклад сесії.
- Після робочої базової версії: GObject, GMainLoop, робочий потік і
  окреме рішення, хто утримує `Task` живим до завершення виконання.

## Питання для незалежного рев’ю

Граничні випадки видачі ID та переповнення `guint`; час життя позичених
результатів; чи тести справді перевіряють обіцяну поведінку (порядок за
`created_at` через публічний API не відрізнити від порядку за ID);
контрактні коментарі в `task_manager.h` короткі й місцями неповні.
