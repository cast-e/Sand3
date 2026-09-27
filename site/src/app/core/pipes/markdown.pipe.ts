import { Pipe, PipeTransform, inject } from '@angular/core';
import { DomSanitizer, SafeHtml } from '@angular/platform-browser';
import { marked } from 'marked';

@Pipe({
  name: 'markdown',
  standalone: true
})
export class MarkdownPipe implements PipeTransform {
  private sanitizer = inject(DomSanitizer);

  transform(content: string | null | undefined): SafeHtml {
    if (!content) return '';
    try {
      const rawHtml = marked.parse(content, { gfm: true, breaks: true }) as string;
      return this.sanitizer.bypassSecurityTrustHtml(rawHtml);
    } catch {
      return content;
    }
  }
}
