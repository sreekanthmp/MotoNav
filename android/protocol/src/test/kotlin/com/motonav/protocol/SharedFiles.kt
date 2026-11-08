package com.motonav.protocol

import java.io.File

/** Reads the files shared with the firmware and the Python tool (repository root, protocol/). */
object SharedFiles {
    val repoRoot: File = File(requireNotNull(System.getProperty("motonav.repoRoot")) { "motonav.repoRoot is not set" })

    fun text(relative: String): String = File(repoRoot, relative).readText(Charsets.UTF_8)

    class Vector(
        val name: String,
        val frame: ByteArray,
        val result: String,
        val fields: Map<String, String>,
        val roadBytes: ByteArray?,
    )

    /** protocol/vectors.txt: `name ; frame ; result [; fields]`. */
    fun vectors(): List<Vector> = text("protocol/vectors.txt").lines()
        .filter { it.isNotBlank() && !it.startsWith("#") }
        .map { line ->
            val parts = line.split(" ; ")
            val fields = HashMap<String, String>()
            var road: ByteArray? = null
            if (parts.size > 3) {
                val text = parts[3]
                val roadAt = text.indexOf("road=")
                for (pair in text.substring(0, roadAt).trim().split(" ")) {
                    val (k, v) = pair.split("=")
                    fields[k] = v
                }
                road = unescape(text.substring(roadAt + "road=".length))
            }
            Vector(parts[0], parseHex(parts[1]), parts[2], fields, road)
        }

    /** protocol/scenarios.txt: `name ; frame`. */
    fun scenarios(): List<Pair<String, ByteArray>> = text("protocol/scenarios.txt").lines()
        .filter { it.isNotBlank() && !it.startsWith("#") }
        .map { line -> line.split(" ; ").let { it[0] to parseHex(it[1]) } }

    /** Printable ASCII as is; every other byte as \xHH. */
    private fun unescape(text: String): ByteArray {
        val out = java.io.ByteArrayOutputStream()
        var i = 0
        while (i < text.length) {
            if (text[i] == '\\' && text.startsWith("x", i + 1)) {
                out.write(text.substring(i + 2, i + 4).toInt(16))
                i += 4
            } else {
                out.write(text[i].code)
                i += 1
            }
        }
        return out.toByteArray()
    }
}
