# Лабораторная 1

Реализации команд cat и grep -> mycat mygrep

### Тестирование

В директории `lab1` находится тестовый файл `TestFile.txt`.

#### mycat

1.  **Просмотр содержимого файла:**

    ```bash
    ./mycat TestFile.txt
    ```

2.  **Нумерация всех строк (`-n`):**

    ```bash
    ./mycat -n TestFile.txt
    ```

3.  **Нумерация непустых строк (`-b`):**

    ```bash
    ./mycat -b TestFile.txt
    ```

4.  **Отображение символа `$` в конце строк (`-E`):**

    ```bash
    ./mycat -E TestFile.txt
    ```

5.  **Комбинация флагов:**

    ```bash
    ./mycat -nE TestFile.txt
    ```

#### mygrep

1.  **Поиск слова в файле:**

    ```bash
    ./mygrep "Hello" TestFile.txt
    ```

2.  **Поиск с использованием `mycat` и pipe:**

    ```bash
    ./mycat TestFile.txt | ./mygrep "lines"
    ```

3.  **Поиск с использованием `ls` и pipe:**

    ```bash
    ls -l | ./mygrep "mycat"
    ```

### Очистка

Для удаления исполняемых файлов выполните команду:

```bash
make clean
```
